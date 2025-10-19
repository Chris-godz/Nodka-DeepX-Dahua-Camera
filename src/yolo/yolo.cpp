#include <algorithm>
#include <opencv2/opencv.hpp>
#include <QDebug>
// #include <utils/common_util.hpp>  // 暂时注释掉版本兼容性问题
#include "yolo.h"
#include "nms.h"

// #define DUMP_DATA

void YoloLayerParam::Show()
{
    std::cout << "    - LayerParam: [ name : " << name << ", " << numGridX << " x " << numGridY << " x " << numBoxes << "boxes" << "], anchorWidth [";
    for(auto &w : anchorWidth) std::cout << w << ", ";
    std::cout << "], anchorHeight [";
    for(auto &h : anchorHeight) std::cout << h << ", ";
    std::cout << "], tensor index [";
    for(auto &t : tensorIdx) std::cout << t << ", ";
    std::cout << "]" << std::endl;
}
void YoloParam::Show()
{
    std::cout << "  YoloParam: " << std::endl << "    - conf_threshold: " << confThreshold << ", "
        << "score_threshold: " << scoreThreshold << ", "
        << "iou_threshold: " << iouThreshold << ", "
        << "num_classes: " << numClasses << ", "
        << "num_layers: " << layers.size() << std::endl;
    for(auto &layer:layers) layer.Show();
    std::cout << "    - classes: [";
    for(auto &c : classNames) std::cout << c << ", ";
    std::cout << "]" << std::endl;
}
Yolo::Yolo() { }
Yolo::~Yolo() { }
Yolo::Yolo(YoloParam &_cfg) :cfg(_cfg)
{
    if(cfg.layers.empty())
    {
        is_onnx_output = true;
    }
    else
    {
        anchorSize = cfg.layers[0].anchorWidth.size();
    }
    
    // Handle automatic box calculation or validate numBoxes
    if(cfg.numBoxes == 0 || cfg.numBoxes == -1)
    {   
        if(cfg.layers.size() > 0)
        {
            cfg.numBoxes = 0;  // Reset to 0 before calculation
            for(auto &layer:cfg.layers)
            {
                cfg.numBoxes += layer.numGridX*layer.numGridY*layer.numBoxes;
            }
            std::cout << "[YOLO] Auto-calculated numBoxes: " << cfg.numBoxes << std::endl;
        }
        else
        {
            std::cerr << "[DXAPP] [WARN] The numBoxes is not set. Please check the numBoxes." << std::endl; 
        }
    }

    // Allocate memory only if numBoxes is valid
    if(cfg.numBoxes > 0 && cfg.numBoxes < 100000)  // Add upper limit check to prevent overflow
    {
        std::cout << "[YOLO] Allocating memory for " << cfg.numBoxes << " boxes" << std::endl;
        //allocate memory
        Boxes = std::vector<float>(cfg.numBoxes*4);
        Keypoints = std::vector<float>(cfg.numBoxes*51);
    }
    else if(cfg.numBoxes >= 100000)
    {
        std::cerr << "[DXAPP] [ERROR] numBoxes value is too large: " << cfg.numBoxes 
                  << ". This may indicate a configuration error." << std::endl;
        throw std::runtime_error("Invalid numBoxes value");
    }
    
    for(size_t i=0; i<cfg.numClasses; i++)
    {
        std::vector<std::pair<float, int>> v;
        ScoreIndices.emplace_back(v);
    }
}

bool Yolo::LayerReorder(dxrt::Tensors output_info)
{
    for(size_t i=0;i<output_info.size();i++)
    {
        if(cfg.onnxOutputName == output_info[i].name())
        {
            cfg.numBoxes = output_info.front().shape()[1];
            std::cout << "cfg.numBoxes: " << cfg.numBoxes << std::endl; 
            onnxOutputIdx.emplace_back(i);
            Boxes.clear();
            Keypoints.clear();
            Boxes = std::vector<float>(cfg.numBoxes*4);
            Keypoints = std::vector<float>(cfg.numBoxes*51);
        }
    }
    if(onnxOutputIdx.size() > 0)
    {
        cfg.Show();
        std::cout << "YOLO created : " << cfg.numBoxes << " boxes, " << cfg.numClasses << " classes, "<< std::endl;
        cfg.layers.clear();
        return true;
    }
    
    std::vector<YoloLayerParam> temp;
    for(size_t i=0;i<output_info.size();i++)
    {   
        for(size_t j=0;j<cfg.layers.size();j++)
        {
            if(output_info[i].name() == cfg.layers[j].name)
            {
                cfg.layers[j].tensorIdx.clear();
                cfg.layers[j].tensorIdx.push_back(static_cast<int32_t>(i));
                temp.emplace_back(cfg.layers[j]);
                break;
            }
        }
    }
    if(temp.size() != output_info.size())
    {
        std::cerr << "[DXAPP] [ER] Yolo::LayerReorder : Output tensor size mismatch. Please check the model output configuration." << std::endl;
        return false;
    }
    if(temp.empty())
    {
        std::cerr << "[DXAPP] [ER] Yolo::LayerReorder : Layer information is missing. This is only supported when USE_ORT=ON. Please modify and rebuild." << std::endl;
        return false;
    }
    cfg.layers.clear();
    cfg.layers = temp;
    cfg.Show();
    return true;
}

static bool scoreComapre(const std::pair<float, int> &a, const std::pair<float, int> &b)
{
    if(a.first > b.first)
        return true;
    else
        return false;
};

std::vector<BoundingBox> Yolo::PostProc(dxrt::TensorPtrs& dataSrc)
{
    static int postprocCount = 0;
    postprocCount++;
    
    qDebug() << "[YOLO POSTPROC] ========== 后处理 #" << postprocCount << " ==========";
    qDebug() << "[YOLO POSTPROC] 输出张量数量:" << dataSrc.size();
    
    if (dataSrc.empty()) {
        qDebug() << "[YOLO POSTPROC ERROR] 输出张量为空！";
        return Result;
    }
    
    // 打印每个输出张量的信息
    for (size_t i = 0; i < dataSrc.size() && i < 10; i++) {
        auto& tensor = dataSrc[i];
        QString shapeStr = "[";
        for (size_t j = 0; j < tensor->shape().size(); j++) {
            shapeStr += QString::number(tensor->shape()[j]);
            if (j < tensor->shape().size() - 1) shapeStr += ",";
        }
        shapeStr += "]";
        qDebug() << "[YOLO POSTPROC] Tensor[" << i << "]: name=" << tensor->name().c_str() 
                 << ", shape=" << shapeStr;
    }
    
    for(size_t label=0; label<static_cast<size_t>(cfg.numClasses); label++) {
        ScoreIndices[label].clear();
    }
    Result.clear();

    qDebug() << "[YOLO POSTPROC] layers.empty() =" << (cfg.layers.empty() ? "true" : "false");
    qDebug() << "[YOLO POSTPROC] postproc_type =" << static_cast<int>(cfg.postproc_type);
    
    if(cfg.layers.empty())
    {
        qDebug() << "[YOLO POSTPROC] 使用 ONNX 后处理模式";
        qDebug() << "[YOLO POSTPROC] 查找输出张量:" << cfg.onnxOutputName.c_str();
        
        bool found = false;
        for(auto &data:dataSrc)
        {
            if(cfg.onnxOutputName == data->name())
            {
                qDebug() << "[YOLO POSTPROC] 找到匹配的输出张量:" << data->name().c_str();
                auto num_elements = data->shape()[1];
                qDebug() << "[YOLO POSTPROC] num_elements =" << num_elements;
                
                onnx_post_processing(dataSrc, num_elements);
                found = true;
                break;
            }
        }
        
        if (!found) {
            qDebug() << "[YOLO POSTPROC ERROR] 未找到匹配的输出张量:" << cfg.onnxOutputName.c_str();
        }
    }
    else
    {
        qDebug() << "[YOLO POSTPROC] 使用原始后处理模式 (layers count:" << cfg.layers.size() << ")";
        raw_post_processing(dataSrc);
    }

    qDebug() << "[YOLO POSTPROC] 排序前候选框统计:";
    int totalCandidates = 0;
    for(size_t label=0 ; label<static_cast<size_t>(cfg.numClasses) ; label++)
    {
        if (ScoreIndices[label].size() > 0) {
            qDebug() << "[YOLO POSTPROC]   类别" << label << ":" << ScoreIndices[label].size() << "个候选";
            totalCandidates += ScoreIndices[label].size();
        }
        sort(ScoreIndices[label].begin(), ScoreIndices[label].end(), scoreComapre);
    }
    qDebug() << "[YOLO POSTPROC] 总候选框数:" << totalCandidates;
    
    qDebug() << "[YOLO POSTPROC] 开始 NMS 处理...";
    qDebug() << "[YOLO POSTPROC] IOU 阈值:" << cfg.iouThreshold;
    
    Nms(
        cfg.numClasses,
        0,
        cfg.classNames, 
        ScoreIndices, Boxes.data(), Keypoints.data(), cfg.iouThreshold,
        Result,
        0
    );

    qDebug() << "[YOLO POSTPROC] NMS 完成，最终检测结果:" << Result.size() << "个目标";
    
    // 打印前几个结果的详情
    for (size_t i = 0; i < Result.size() && i < 3; i++) {
        const auto& box = Result[i];
        qDebug() << "[YOLO POSTPROC]   结果[" << i << "]: label=" << box.label 
                 << "(" << box.labelname.c_str() << "), score=" << box.score
                 << ", box=[" << box.box[0] << "," << box.box[1] << "," << box.box[2] << "," << box.box[3] << "]";
    }
    if (Result.size() > 3) {
        qDebug() << "[YOLO POSTPROC]   ... 还有" << (Result.size() - 3) << "个结果";
    }

    return Result;
}

// 新增：用於處理直接輸出到 buffer 的 PostProc 版本
// 參考：dx_app-1.11.0/demos/object_detection/yolo.cpp:438-526
std::vector<BoundingBox> Yolo::PostProc(void* data, std::vector<std::vector<int64_t>> output_shape, dxrt::DataType data_type, int output_length)
{
    qDebug() << "[YOLO POSTPROC BUFFER] ========== 使用 buffer 版本後處理 ==========";
    qDebug() << "[YOLO POSTPROC BUFFER] data pointer:" << data;
    qDebug() << "[YOLO POSTPROC BUFFER] output_shape.size():" << output_shape.size();
    qDebug() << "[YOLO POSTPROC BUFFER] data_type:" << static_cast<int>(data_type);
    qDebug() << "[YOLO POSTPROC BUFFER] output_length:" << output_length;
    
    std::vector<BoundingBox> result;
    
    // 清空之前的結果
    for(int cls=0; cls<(int)cfg.numClasses; cls++)
    {
        ScoreIndices[cls].clear();
    }
    Result.clear();
    
    // 根據 output_shape 判斷處理方式（不依賴 data_type）
    if (output_shape.size() > 1)
    {
        qDebug() << "[YOLO POSTPROC BUFFER] 處理多層輸出（anchor-based YOLO），層數:" << output_shape.size();
        // 調用 FilterWithSort 處理原始 buffer
        FilterWithSort(data, output_shape, data_type);
        
        qDebug() << "[YOLO POSTPROC BUFFER] FilterWithSort 完成，開始 NMS...";
        
        // 排序
        for(int cls=0; cls<(int)cfg.numClasses; cls++)
        {
            sort(ScoreIndices[cls].begin(), ScoreIndices[cls].end(), scoreComapre);
        }
        
        // NMS
        Nms(
            cfg.numClasses,
            0,
            cfg.classNames, 
            ScoreIndices, Boxes.data(), Keypoints.data(), cfg.iouThreshold,
            result,
            0
        );
        
        qDebug() << "[YOLO POSTPROC BUFFER] NMS 完成，檢測到" << result.size() << "個目標";
    }
    else
    {
        qDebug() << "[YOLO POSTPROC BUFFER] 單層輸出（簡化處理）";
        // 簡化處理：直接當作 float* 處理
        return result;
    }
    
    Result = result;
    return result;
}

void Yolo::onnx_post_processing(dxrt::TensorPtrs &outputs, int64_t num_elements) {
    std::cout << "[YOLO ONNX_POST] 开始 ONNX 后处理" << std::endl;
    std::cout << "[YOLO ONNX_POST] num_elements = " << num_elements << std::endl;
    std::cout << "[YOLO ONNX_POST] scoreThreshold = " << cfg.scoreThreshold << std::endl;
    std::cout << "[YOLO ONNX_POST] confThreshold = " << cfg.confThreshold << std::endl;
    std::cout << "[YOLO ONNX_POST] 期望的 onnxOutputName = '" << cfg.onnxOutputName << "'" << std::endl;
    std::cout << "[YOLO ONNX_POST] 实际输出张量数量 = " << outputs.size() << std::endl;
    
    // **新增：打印所有输出张量的详细信息**
    for(size_t i = 0; i < outputs.size(); ++i) {
        auto &tensor = outputs[i];
        std::cout << "[YOLO ONNX_POST] 张量 [" << i << "]: 名称='" << tensor->name() << "'";
        std::cout << ", 形状=[";
        for(size_t j = 0; j < tensor->shape().size(); ++j) {
            std::cout << tensor->shape()[j];
            if(j < tensor->shape().size() - 1) std::cout << ",";
        }
        std::cout << "]" << std::endl;
    }
    
    // 查找匹配 onnxOutputName 的张量
    dxrt::TensorPtr matchedTensor = nullptr;
    for(auto &tensor : outputs) {
        if(tensor->name() == cfg.onnxOutputName) {
            matchedTensor = tensor;
            std::cout << "[YOLO ONNX_POST] ✅ 找到匹配张量: " << tensor->name() << std::endl;
            break;
        }
    }
    
    if(!matchedTensor) {
        std::cerr << "[YOLO ONNX_POST ERROR] ❌ 未找到匹配的输出张量: '" << cfg.onnxOutputName << "'" << std::endl;
        std::cerr << "[YOLO ONNX_POST ERROR] 请检查上面列出的实际张量名称" << std::endl;
        return;
    }
    
    int x = 0, y = 1, w = 2, h = 3;
    float scoreThreshold = cfg.scoreThreshold;
    float conf_threshold = cfg.confThreshold;
    auto *dataSrc = static_cast<void*>(matchedTensor->data());
    auto data_pitch_size = matchedTensor->shape()[2];
    cv::Mat raw_data;
    int class_index = 5;
    
    std::cout << "[YOLO ONNX_POST] 初始 data_pitch_size = " << data_pitch_size << std::endl;
    std::cout << "[YOLO ONNX_POST] postproc_type = " << static_cast<int>(cfg.postproc_type) << std::endl;
    
    if(cfg.postproc_type == PostProcType::YOLOV8) 
    {
        std::cout << "[YOLO ONNX_POST] 使用 YOLOv8 模式" << std::endl;
        data_pitch_size = matchedTensor->shape()[1];
        num_elements = matchedTensor->shape()[2];
        std::cout << "[YOLO ONNX_POST] YOLOv8: data_pitch_size = " << data_pitch_size << ", num_elements = " << num_elements << std::endl;
        
        raw_data = cv::Mat(data_pitch_size, num_elements, CV_32F, (float*)dataSrc);
        raw_data = raw_data.t();
        dataSrc = static_cast<void*>(raw_data.data);
        class_index = 4;
        std::cout << "[YOLO ONNX_POST] YOLOv8: 转置后矩阵 " << raw_data.rows << "x" << raw_data.cols << std::endl;
    }

    int validBoxes = 0;
    int highConfBoxes = 0;
    
    for(int boxIdx=0;boxIdx<num_elements;boxIdx++)
    {
        auto *data = static_cast<float*>(dataSrc) + (data_pitch_size * boxIdx);
        auto obj_conf = data[4];
        if(cfg.postproc_type == PostProcType::YOLOV8)
        {
            obj_conf = 1.f;
        }
        if(obj_conf>conf_threshold)
        {
            highConfBoxes++;
            
            int max_cls = -1;
            float max_score = scoreThreshold;
            for(int cls=0; cls<(int)cfg.numClasses; cls++)
            {
                auto cls_data = data[class_index + cls];
                auto cls_conf = obj_conf * cls_data;
                if(cls_conf > max_score)
                {
                    max_cls = cls;
                    max_score = cls_conf;
                }
                else continue;
            }
            if(max_cls > -1)
            {
                validBoxes++;
                ScoreIndices[max_cls].emplace_back(max_score, boxIdx);
                Boxes[boxIdx*4+0] = data[x] - data[w] / 2.; /*x1*/
                Boxes[boxIdx*4+1] = data[y] - data[h] / 2.; /*y1*/
                Boxes[boxIdx*4+2] = data[x] + data[w] / 2.; /*x2*/
                Boxes[boxIdx*4+3] = data[y] + data[h] / 2.; /*y2*/

                switch(cfg.postproc_type)
                {
                    case PostProcType::POSE: // POSE
                        for(int k = 0; k < 17; k++)
                        {
                            int kptIdx = (k * 3) + 6;
                            Keypoints[boxIdx*51+k*3+0] = data[kptIdx + 0];
                            Keypoints[boxIdx*51+k*3+1] = data[kptIdx + 1];
                            Keypoints[boxIdx*51+k*3+2] = data[kptIdx + 2];
                        }
                        break;
                    case PostProcType::FACE: // FACE
                        for(int k = 0; k < 5; k++)
                        {
                            int kptIdx = (k * 2) + 5;
                            Keypoints[boxIdx*51+k*3+0] = data[kptIdx + 0];
                            Keypoints[boxIdx*51+k*3+1] = data[kptIdx + 1];
                            Keypoints[boxIdx*51+k*3+2] = 0.5;
                        }
                        break;
                    default:
                        break;
                }
            }
            else continue;
        }
    }
    
    std::cout << "[YOLO ONNX_POST] ========== 后处理完成 ==========" << std::endl;
    std::cout << "[YOLO ONNX_POST] 总检测框数: " << num_elements << std::endl;
    std::cout << "[YOLO ONNX_POST] 高置信度框 (>" << conf_threshold << "): " << highConfBoxes << std::endl;
    std::cout << "[YOLO ONNX_POST] 有效检测框 (有类别): " << validBoxes << std::endl;
}

void Yolo::raw_post_processing(dxrt::TensorPtrs outputs) {  // 改为按值传递
    qDebug() << "[YOLO RAW_POST] ========== 开始 RAW 后处理 ==========";
    qDebug() << "[YOLO RAW_POST] 输出张量数量 =" << outputs.size();
    
    // **新增：打印所有输出张量的详细信息**
    for(size_t i = 0; i < outputs.size(); ++i) {
        auto &tensor = outputs[i];
        QString shapeStr = "[";
        for(size_t j = 0; j < tensor->shape().size(); ++j) {
            shapeStr += QString::number(tensor->shape()[j]);
            if(j < tensor->shape().size() - 1) shapeStr += ",";
        }
        shapeStr += "]";
        qDebug() << "[YOLO RAW_POST] 张量 [" << i << "]: 名称='" << tensor->name().c_str() 
                 << "', 形状=" << shapeStr;
    }
    
    qDebug() << "[YOLO RAW_POST] postproc_type =" << static_cast<int>(cfg.postproc_type);
    qDebug() << "[YOLO RAW_POST] layers.size() =" << cfg.layers.size();
    
    int boxIdx = 0;
    int x = 0, y = 1, w = 2, h = 3;
    std::vector<float> box_temp(4);
    if(cfg.postproc_type == PostProcType::YOLOV8)
    {
        // std::cout << "[ERROR] YOLOv8 mode is not supported." << std::endl;
        int scores_tensor_idx = cfg.layers[0].tensorIdx[0];
        int boxes_tensor_idx = cfg.layers[1].tensorIdx[0];
        float* boxes_output_tensor = static_cast<float*>(outputs[boxes_tensor_idx]->data());
        float* scores_output_tensor = static_cast<float*>(outputs[scores_tensor_idx]->data());
        int boxes_pitch_size = outputs[boxes_tensor_idx]->shape()[3];
        int score_pitch_size = outputs[scores_tensor_idx]->shape()[2];
        std::vector<int> feature_strides = {8, 16, 32};
        int index = -1;
        for(int i=0;i<(int)feature_strides.size();i++)
        {
            int stride = feature_strides[i];
            int numGridX = cfg.width / stride;
            int numGridY = cfg.height / stride;
            for(int gY=0; gY<numGridY; gY++)
            {
                for(int gX=0; gX<numGridX; gX++)
                {
                    index++;
                    int max_cls = -1;
                    float max_score = cfg.scoreThreshold;
                    for(int cls=0;cls<static_cast<int>(cfg.numClasses);cls++)
                    {
                        float class_score = scores_output_tensor[(cls * score_pitch_size) + index];
                        if(class_score > max_score)
                        {
                            max_cls = cls;
                            max_score = class_score;
                        }
                    }
                    if(max_cls > -1)
                    {
                        ScoreIndices[max_cls].emplace_back(max_score, boxIdx);
                        std::vector<float> data(4);
                        float _605output01 = boxes_output_tensor[(0 * boxes_pitch_size) + index];
                        float _605output02 = boxes_output_tensor[(1 * boxes_pitch_size) + index];
                        float _608output01 = boxes_output_tensor[(2 * boxes_pitch_size) + index];
                        float _608output02 = boxes_output_tensor[(3 * boxes_pitch_size) + index];

                        float _605output01_s = (_605output01 * (-1) + (0.5f + gX));
                        float _605output02_s = (_605output02 * (-1) + (0.5f + gY));
                        float _608output01_s = (_608output01 + (0.5f + gX));
                        float _608output02_s = (_608output02 + (0.5f + gY));

                        _605output01 = _608output01_s - _605output01_s;
                        _605output02 = _608output02_s - _605output02_s;
                        _608output01 = (_608output01_s + _605output01_s) * 0.5; // 613
                        _608output02 = (_608output02_s + _605output02_s) * 0.5; // 613

                        data[0] = _608output01 * stride;
                        data[1] = _608output02 * stride;
                        data[2] = _605output01 * stride;
                        data[3] = _605output02 * stride;
                        Boxes[boxIdx*4+0] = data[0] - data[2]/2.f;
                        Boxes[boxIdx*4+1] = data[1] - data[3]/2.f;
                        Boxes[boxIdx*4+2] = data[0] + data[2]/2.f;
                        Boxes[boxIdx*4+3] = data[1] + data[3]/2.f;
                        boxIdx++;
                    }
                }
            }

        }
    }
    else if(anchorSize > 0)
    {
        qDebug() << "[YOLO RAW_POST] 使用传统 anchor-based YOLO 处理";
        qDebug() << "[YOLO RAW_POST] 开始遍历" << cfg.layers.size() << "个层";
        
        int layerCount = 0;
        for(auto &layer:cfg.layers)        
        {
            layerCount++;
            qDebug() << "[YOLO RAW_POST] ==== 处理层" << layerCount << "/" << cfg.layers.size() << "====";
            qDebug() << "[YOLO RAW_POST] 层名称:" << layer.name.c_str();
            qDebug() << "[YOLO RAW_POST] numGridX =" << layer.numGridX << ", numGridY =" << layer.numGridY;
            qDebug() << "[YOLO RAW_POST] tensorIdx[0] =" << layer.tensorIdx[0];
            qDebug() << "[YOLO RAW_POST] anchorWidth.size() =" << layer.anchorWidth.size();
            
            int strideX = cfg.width / layer.numGridX;
            int strideY = cfg.height / layer.numGridY;
            int numGridX = layer.numGridX;
            int numGridY = layer.numGridY;
            int tensorIdx = layer.tensorIdx[0];
            
            qDebug() << "[YOLO RAW_POST] strideX =" << strideX << ", strideY =" << strideY;
            qDebug() << "[YOLO RAW_POST] 准备访问 outputs[" << tensorIdx << "]";
            
            if (tensorIdx >= (int)outputs.size()) {
                qDebug() << "[YOLO RAW_POST ERROR] tensorIdx 越界！tensorIdx =" << tensorIdx << ", outputs.size() =" << outputs.size();
                continue;
            }
            
            qDebug() << "[YOLO RAW_POST] outputs.size() =" << outputs.size();
            qDebug() << "[YOLO RAW_POST] 获取 outputs[" << tensorIdx << "] 指针...";
            
            // 先检查指针是否为空
            auto tensor = outputs[tensorIdx];
            if (!tensor) {
                qDebug() << "[YOLO RAW_POST ERROR] outputs[" << tensorIdx << "] 是空指针！";
                continue;
            }
            
            qDebug() << "[YOLO RAW_POST] 张量指针有效，调用 shape()...";
            
            // 检查张量的实际形状 - 使用 try-catch 保护
            std::vector<int64_t> shape;
            try {
                shape = tensor->shape();
                qDebug() << "[YOLO RAW_POST] shape() 调用成功";
            } catch (...) {
                qDebug() << "[YOLO RAW_POST ERROR] shape() 调用失败！";
                continue;
            }
            
            // 打印张量形状
            QString shapeStr = "[";
            for (size_t i = 0; i < shape.size(); i++) {
                shapeStr += QString::number(shape[i]);
                if (i < shape.size() - 1) shapeStr += ",";
            }
            shapeStr += "]";
            qDebug() << "[YOLO RAW_POST] 张量实际形状:" << shapeStr;
            
            // 檢查張量維度
            if (shape.size() != 4) {
                qDebug() << "[YOLO RAW_POST ERROR] 張量維度不是4，跳過該層";
                continue;
            }
            
            int tensorChannels = shape[3];  // [1, H, W, C] 格式
            int expectedChannels = layer.anchorWidth.size() * (cfg.numClasses + 5);
            
            qDebug() << "[YOLO RAW_POST] 張量通道數:" << tensorChannels;
            qDebug() << "[YOLO RAW_POST] 期望最小通道數:" << expectedChannels << "(anchors=" << layer.anchorWidth.size() << "* (classes=" << cfg.numClasses << "+ 5))";
            
            // 移除嚴格的通道數檢查，因為YoloV7等模型可能有額外的通道(如256而不是255)
            // 只要通道數足夠容納所需數據即可
            if (tensorChannels < expectedChannels) {
                qDebug() << "[YOLO RAW_POST ERROR] 張量通道數不足！實際:" << tensorChannels << ", 需要:" << expectedChannels;
                qDebug() << "[YOLO RAW_POST ERROR] 跳過該層的處理";
                continue;
            }
            
            if (tensorChannels > expectedChannels) {
                qDebug() << "[YOLO RAW_POST] 注意: 張量有額外通道 (" << tensorChannels << " > " << expectedChannels << "), 這是正常的（如YoloV7的256通道格式）";
            }
            
            float scale_x_y = layer.scaleX;
            
            qDebug() << "[YOLO RAW_POST] 开始处理网格，numBoxes(anchors) =" << layer.anchorWidth.size();
            qDebug() << "[YOLO RAW_POST] 张量通道数:" << shape[3];
            qDebug() << "[YOLO RAW_POST] 每个box需要的通道数:" << (cfg.numClasses + 5);
            
            qDebug() << "[YOLO RAW_POST] 开始遍历网格...";
            
            // 獲取張量原始數據指針（無參數版本）
            // 參考 dx_app-1.11.0/demos/object_detection/yolo.cpp:181-183
            float* tensor_data = static_cast<float*>(tensor->data());
            if (!tensor_data) {
                qDebug() << "[YOLO RAW_POST ERROR] tensor->data() 返回空指針！";
                continue;
            }
            qDebug() << "[YOLO RAW_POST] ✓ 成功獲取張量數據指針:" << (void*)tensor_data;
            
            // 遍历网格和anchor boxes
            for(int gY=0; gY<numGridY; gY++)
            {
                for(int gX=0; gX<numGridX; gX++)
                {
                    for(int box=0; box<(int)layer.anchorWidth.size(); box++)
                    { 
                        bool boxDecoded = false;
                        
                        // 手動計算偏移量（參考 dx_app-1.11.0/demos/object_detection/yolo.cpp:181-183）
                        // data = output_per_layers + (gY * numGridX * channels) + (gX * channels) + (box * (numClasses + 5))
                        int offset = (gY * numGridX * tensorChannels) + (gX * tensorChannels) + (box * (cfg.numClasses + 5));
                        float *data = tensor_data + offset;
                        
                        // 第一次迭代時打印調試信息
                        if (gY == 0 && gX == 0 && box == 0) {
                            qDebug() << "[YOLO RAW_POST] 第一個box偏移計算:";
                            qDebug() << "[YOLO RAW_POST]   gY=" << gY << ", gX=" << gX << ", box=" << box;
                            qDebug() << "[YOLO RAW_POST]   numGridX=" << numGridX << ", tensorChannels=" << tensorChannels;
                            qDebug() << "[YOLO RAW_POST]   cfg.numClasses=" << cfg.numClasses;
                            qDebug() << "[YOLO RAW_POST]   計算偏移=" << offset;
                            qDebug() << "[YOLO RAW_POST]   data指針=" << (void*)data;
                            qDebug() << "[YOLO RAW_POST]   data[0]=" << data[0] << ", data[4]=" << data[4];
                        }
                        
                        // objectness 在偏移 4 的位置
                        float rawObjectness = data[4];
                        float rawThreshold = log(cfg.confThreshold / (1 - cfg.confThreshold));
                        
                        // 快速检查：如果 raw 值太小，直接跳过
                        if(rawObjectness <= rawThreshold)
                        {
                            boxIdx++;
                            continue;
                        }
                        
                        float objectness_score = sigmoid(rawObjectness);
                        
                        if(objectness_score > cfg.confThreshold)
                        {
                            int max_cls = -1;
                            float max_score = cfg.scoreThreshold;
                            for(int cls=0; cls<(int)cfg.numClasses;cls++)
                            {   
                                // 类别置信度在偏移 5+cls 的位置
                                float cls_conf = objectness_score * sigmoid(data[5+cls]);
                                if(cls_conf > max_score)
                                {
                                    max_cls = cls;
                                    max_score = cls_conf;
                                }
                            }
                            
                            if(max_cls > -1)
                            {
                                ScoreIndices[max_cls].emplace_back(max_score, boxIdx);
                                if(!boxDecoded)
                                {
                                    // 边界框坐标在偏移 0,1,2,3 的位置
                                    if(scale_x_y==0)
                                    {
                                        box_temp[x] = ( sigmoid(data[x]) * 2. - 0.5 + gX ) * strideX;
                                        box_temp[y] = ( sigmoid(data[y]) * 2. - 0.5 + gY ) * strideY;
                                    }
                                    else
                                    {
                                        box_temp[x] = (sigmoid(data[x] * scale_x_y  - 0.5 * (scale_x_y - 1)) + gX) * strideX;
                                        box_temp[y] = (sigmoid(data[y] * scale_x_y  - 0.5 * (scale_x_y - 1)) + gY) * strideY;
                                    }
                                    box_temp[w] = pow((sigmoid(data[w]) * 2.), 2) * layer.anchorWidth[box];
                                    box_temp[h] = pow((sigmoid(data[h]) * 2.), 2) * layer.anchorHeight[box];
                                    Boxes[boxIdx*4+0] = box_temp[x] - box_temp[w] / 2.; /*x1*/
                                    Boxes[boxIdx*4+1] = box_temp[y] - box_temp[h] / 2.; /*y1*/
                                    Boxes[boxIdx*4+2] = box_temp[x] + box_temp[w] / 2.; /*x2*/
                                    Boxes[boxIdx*4+3] = box_temp[y] + box_temp[h] / 2.; /*y2*/
                                    boxDecoded = true;
                                }
                            }
                        }
                        boxIdx++;
                    }
                }
            }
        }
    }
}

// 新增：FilterWithSort for void* buffer version
// 參考：dx_app-1.11.0/demos/object_detection/yolo.cpp:143-305
void Yolo::FilterWithSort(void* outputs, std::vector<std::vector<int64_t>> output_shape, dxrt::DataType data_type)
{
    qDebug() << "[YOLO FILTER BUFFER] ========== FilterWithSort buffer 版本 ==========";
    qDebug() << "[YOLO FILTER BUFFER] output_shape.size():" << output_shape.size();
    qDebug() << "[YOLO FILTER BUFFER] cfg.layers.size():" << cfg.layers.size();
    
    int boxIdx = 0;
    int x = 0, y = 1, w = 2, h = 3;
    float ScoreThreshold = cfg.scoreThreshold;
    float conf_threshold = cfg.confThreshold;
    float rawThreshold = log(conf_threshold/(1-conf_threshold));
    float score, score1, box_temp[4];
    float *data;
    float* output_per_layers = (float*)outputs;
    
    if(anchorSize > 0)  // anchor-based YOLO (YOLOv5, YOLOv7 etc.)
    {
        qDebug() << "[YOLO FILTER BUFFER] 使用 anchor-based 處理";
        
        for(size_t i=0; i<cfg.layers.size(); i++)
        {
            auto layer = cfg.layers[i];
            int strideX = cfg.width / layer.numGridX;
            int strideY = cfg.height / layer.numGridY;
            int numGridX = layer.numGridX;
            int numGridY = layer.numGridY;
            int tensorIdx = layer.tensorIdx[0];
            float scale_x_y = layer.scaleX;
            
            // 計算當前層在 buffer 中的起始位置
            int layer_pitch = 1;
            if(i > 0)
            {
                for(const auto &s : output_shape[i-1])
                {
                    layer_pitch *= s;
                }
                output_per_layers += layer_pitch;
            }
            
            qDebug() << "[YOLO FILTER BUFFER] 處理層" << i << ":" << layer.name.c_str()
                     << ", grid:" << numGridX << "x" << numGridY
                     << ", stride:" << strideX << "x" << strideY
                     << ", anchors:" << layer.numBoxes;
            
            int channels = output_shape[i].back();  // 最後一維是通道數
            
            for(int gY=0; gY<numGridY; gY++)
            {
                for(int gX=0; gX<numGridX; gX++)
                {
                    for(int box=0; box<layer.numBoxes; box++)
                    { 
                        bool boxDecoded = false;
                        
                        // 手動計算偏移（與之前修復的 raw_post_processing 一致）
                        data = output_per_layers + (gY * numGridX * channels)
                                                 + (gX * channels)
                                                 + (box * (cfg.numClasses + 5));
                        
                        // 快速檢查 objectness
                        if(data[4] > rawThreshold)
                        {
                            score1 = sigmoid(data[4]);
                            
                            if(score1 > conf_threshold)
                            {
                                for(int cls=0; cls<(int)cfg.numClasses; cls++)
                                {
                                    score = score1 * sigmoid(data[5+cls]);
                                    
                                    if (score > ScoreThreshold)
                                    {
                                        ScoreIndices[cls].emplace_back(score, boxIdx);
                                        
                                        if(!boxDecoded)
                                        {
                                            if(scale_x_y==0)
                                            {
                                                box_temp[x] = ( sigmoid(data[x]) * 2. - 0.5 + gX ) * strideX;
                                                box_temp[y] = ( sigmoid(data[y]) * 2. - 0.5 + gY ) * strideY;
                                            }
                                            else
                                            {
                                                box_temp[x] = (sigmoid(data[x] * scale_x_y  - 0.5 * (scale_x_y - 1)) + gX) * strideX;
                                                box_temp[y] = (sigmoid(data[y] * scale_x_y  - 0.5 * (scale_x_y - 1)) + gY) * strideY;
                                            }
                                            box_temp[w] = pow((sigmoid(data[w]) * 2.), 2) * layer.anchorWidth[box];
                                            box_temp[h] = pow((sigmoid(data[h]) * 2.), 2) * layer.anchorHeight[box];
                                            Boxes[boxIdx*4+0] = box_temp[x] - box_temp[w] / 2.; /*x1*/
                                            Boxes[boxIdx*4+1] = box_temp[y] - box_temp[h] / 2.; /*y1*/
                                            Boxes[boxIdx*4+2] = box_temp[x] + box_temp[w] / 2.; /*x2*/
                                            Boxes[boxIdx*4+3] = box_temp[y] + box_temp[h] / 2.; /*y2*/
                                            boxDecoded = true;
                                        }
                                    }
                                }
                            }
                        }
                        boxIdx++;
                    }
                }
            }
            
            qDebug() << "[YOLO FILTER BUFFER] 層" << i << "處理完成，當前總 box 數:" << boxIdx;
        }
    }
    
    qDebug() << "[YOLO FILTER BUFFER] FilterWithSort 完成，總處理 box:" << boxIdx;
}

