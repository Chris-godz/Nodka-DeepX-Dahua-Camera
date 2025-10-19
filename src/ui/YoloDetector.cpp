#include "YoloDetector.h"
#include "yolo/image.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <dxrt/dxrt_api.h>  // 添加 DXRT API 头文件以获取 dxrt::Exception
// #include <utils/common_util.hpp>  // 暂时注释掉版本检查功能

// 外部声明的 YOLO 参数配置
extern YoloParam yolov5s_320, yolov5s_512, yolov5s_640, 
                 yolov7_512, yolov7_640, yolov8_640, 
                 yolox_s_512, yolov5s_face_640, yolov3_512, 
                 yolov4_416, yolov9_640;

// 在全局作用域添加调试函数
static void debugYoloParams() {
    qDebug() << "[YOLO DEBUG] 直接访问 extern 变量:";
    qDebug() << "[YOLO DEBUG] yolov9_640.width =" << yolov9_640.width;
    qDebug() << "[YOLO DEBUG] yolov9_640.height =" << yolov9_640.height;
    qDebug() << "[YOLO DEBUG] yolov9_640.numClasses =" << yolov9_640.numClasses;
    qDebug() << "[YOLO DEBUG] yolov9_640.numBoxes =" << yolov9_640.numBoxes;
}

// 直接使用指针数组引用 extern 变量，避免拷贝构造问题
static YoloParam* g_yoloParamsPtr[] = {
    &yolov5s_320,      // 0
    &yolov5s_512,      // 1
    &yolov5s_640,      // 2 - 默认
    &yolov7_512,       // 3
    &yolov7_640,       // 4
    &yolov8_640,       // 5
    &yolox_s_512,      // 6
    &yolov5s_face_640, // 7
    &yolov3_512,       // 8
    &yolov4_416,       // 9
    &yolov9_640        // 10
};
static const int g_yoloParamsCount = sizeof(g_yoloParamsPtr) / sizeof(g_yoloParamsPtr[0]);

YoloDetector::YoloDetector(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
{
}

YoloDetector::~YoloDetector()
{
    QMutexLocker locker(&m_mutex);
    m_inferenceEngine.reset();
    m_yolo.reset();
}

bool YoloDetector::initializeModel(const QString& modelPath, int parameterIndex)
{
    QMutexLocker locker(&m_mutex);

    try {
        qDebug() << "[YOLO] 开始初始化模型:" << modelPath;
        qDebug() << "[YOLO] 参数索引:" << parameterIndex;
        
        // 调试：打印 extern 变量的值
        debugYoloParams();
        
        // 验证参数索引
        if (parameterIndex < 0 || parameterIndex >= g_yoloParamsCount) {
            QString error = QString("Invalid parameter index: %1. Valid range: 0-%2")
                .arg(parameterIndex)
                .arg(g_yoloParamsCount - 1);
            qWarning() << "[YOLO ERROR]" << error;
            emit errorOccurred(error);
            return false;
        }

        // 获取配置（通过指针引用）
        m_config = *g_yoloParamsPtr[parameterIndex];
        qDebug() << "[YOLO] 从 g_yoloParamsPtr[" << parameterIndex << "] 复制配置";
        qDebug() << "[YOLO] g_yoloParamsPtr[" << parameterIndex << "]->width =" << g_yoloParamsPtr[parameterIndex]->width;
        qDebug() << "[YOLO] g_yoloParamsPtr[" << parameterIndex << "]->height =" << g_yoloParamsPtr[parameterIndex]->height;
        qDebug() << "[YOLO] m_config.width =" << m_config.width;
        qDebug() << "[YOLO] m_config.height =" << m_config.height;
        qDebug() << "[YOLO] m_config.numClasses =" << m_config.numClasses;
        qDebug() << "[YOLO] m_config.numBoxes =" << m_config.numBoxes;

        // 尝试不使用 InferenceOption，直接用默认配置
        qDebug() << "[YOLO] 尝试使用默认 InferenceOption...";

        // 创建推理引擎
        qDebug() << "[YOLO] 正在创建推理引擎...";
        qDebug() << "[YOLO] 模型路径:" << modelPath;
        
        // 检查模型文件
        QFileInfo modelFileInfo(modelPath);
        if (!modelFileInfo.exists()) {
            QString error = QString("模型文件不存在: %1").arg(modelPath);
            qCritical() << "[YOLO ERROR]" << error;
            emit errorOccurred(error);
            return false;
        }
        qDebug() << "[YOLO] 模型文件大小:" << modelFileInfo.size() << "bytes";
        qDebug() << "[YOLO] 模型文件可读:" << modelFileInfo.isReadable();
        
        // 使用传入的模型路径参数
        QString relativeModelPath = modelPath;
        
        qDebug() << "[YOLO] 工作目录:" << QDir::currentPath();
        qDebug() << "[YOLO] 绝对路径:" << modelFileInfo.absoluteFilePath();
        qDebug() << "[YOLO] 使用模型路径:" << relativeModelPath;
        
        std::string modelPathStd = relativeModelPath.toStdString();
        qDebug() << "[YOLO] std::string路径:" << QString::fromStdString(modelPathStd);
        
        // 打印当前工作目录信息（加载模型前）
        qDebug() << "[YOLO] ========== 准备加载模型 ==========";
        qDebug() << "[YOLO] QDir::currentPath():" << QDir::currentPath();
        qDebug() << "[YOLO] 模型文件是否存在:" << QFile::exists(relativeModelPath);
        if (QFile::exists(relativeModelPath)) {
            QFileInfo checkFile(relativeModelPath);
            qDebug() << "[YOLO] 相对路径文件大小:" << checkFile.size() << "bytes";
            qDebug() << "[YOLO] 相对路径文件绝对路径:" << checkFile.absoluteFilePath();
        }
        qDebug() << "[YOLO] 传递给 InferenceEngine 的路径:" << QString::fromStdString(modelPathStd);
        qDebug() << "[YOLO] ======================================";
        
        try {
            qDebug() << "[YOLO] 开始调用 InferenceEngine 构造函数(使用相对路径)...";
            qDebug() << "[YOLO] 尝试使用 new 而非 make_unique...";
            
            // 尝试使用普通 new 代替 make_unique
            dxrt::InferenceEngine* engine = nullptr;
            try {
                qDebug() << "[YOLO] 创建 InferenceEngine 对象...";
                engine = new dxrt::InferenceEngine(modelPathStd);
                qDebug() << "[YOLO] ✓ InferenceEngine 对象创建成功";
                
                // 包装到 unique_ptr
                m_inferenceEngine.reset(engine);
                qDebug() << "[YOLO] ✓ 推理引擎已设置到 unique_ptr";
            }
            catch (...) {
                qCritical() << "[YOLO] InferenceEngine 构造函数抛出异常";
                if (engine) {
                    delete engine;
                }
                throw;
            }
        }
        catch (const dxrt::Exception& e) {
            QString errorMsg = QString("推理引擎创建失败 [dxrt::Exception]: %1 (错误码: %2)")
                .arg(e.what())
                .arg(e.code());
            qCritical() << "[YOLO EXCEPTION - DXRT]" << errorMsg;
            qCritical() << "[YOLO] 异常详情:" << e.what();
            qCritical() << "[YOLO] 错误代码:" << e.code();
            
            emit errorOccurred(errorMsg);
            m_inferenceEngine.reset();
            m_yolo.reset();
            return false;
        }
        catch (const std::exception& e) {
            QString errorMsg = QString("推理引擎创建失败 [std::exception]: %1").arg(e.what());
            qCritical() << "[YOLO EXCEPTION]" << errorMsg;
            qCritical() << "[YOLO] 异常类型:" << typeid(e).name();
            emit errorOccurred(errorMsg);
            m_inferenceEngine.reset();
            m_yolo.reset();
            return false;
        }
        catch (...) {
            QString errorMsg = "推理引擎创建失败: 未知异常类型";
            qCritical() << "[YOLO EXCEPTION]" << errorMsg;
            qCritical() << "[YOLO] 可能原因: 1)DXRT运行时环境问题 2)模型文件损坏 3)设备驱动问题";
            emit errorOccurred(errorMsg);
            m_inferenceEngine.reset();
            m_yolo.reset();
            return false;
        }

        // 暂时跳过版本检查
        // if (!dxapp::common::minversionforRTandCompiler(m_inferenceEngine.get())) {
        //     QString error = "模型编译版本与运行时版本不兼容，请重新编译模型";
        //     qWarning() << error;
        //     emit errorOccurred(error);
        //     return false;
        // }

        // 创建 YOLO 处理器
        qDebug() << "[YOLO] 正在创建 YOLO 处理器...";
        m_yolo = std::make_unique<Yolo>(m_config);
        qDebug() << "[YOLO] YOLO 处理器创建成功";
        
        // 重新排序层
        qDebug() << "[YOLO] 正在重排序层...";
        
        // 打印所有输出张量信息以便调试
        auto outputs = m_inferenceEngine->GetOutputs();
        qDebug() << "[YOLO] 模型输出张量数量:" << outputs.size();
        for (size_t i = 0; i < outputs.size(); i++) {
            QString shapeStr = "[";
            for (size_t j = 0; j < outputs[i].shape().size(); j++) {
                shapeStr += QString::number(outputs[i].shape()[j]);
                if (j < outputs[i].shape().size() - 1) shapeStr += ",";
            }
            shapeStr += "]";
            qDebug() << "[YOLO] 输出张量[" << i << "]: name =" << outputs[i].name().c_str() 
                     << ", shape =" << shapeStr;
        }
        qDebug() << "[YOLO] 配置的 onnxOutputName =" << m_config.onnxOutputName.c_str();
        
        if (!m_yolo->LayerReorder(outputs)) {
            QString error = "YOLO层重排序失败";
            qWarning() << "[YOLO ERROR]" << error;
            emit errorOccurred(error);
            return false;
        }
        qDebug() << "[YOLO] 层重排序成功";

        // 分配输出缓冲区
        qDebug() << "[YOLO] 分配输出缓冲区...";
        m_outputBuffer.resize(m_inferenceEngine->GetOutputSize());
        qDebug() << "[YOLO] 输出缓冲区大小:" << m_outputBuffer.size();
        
        // 分配预处理图像缓冲区
        qDebug() << "[YOLO] 分配预处理图像缓冲区...";
        m_preprocessedImage = cv::Mat(m_config.height, m_config.width, CV_8UC3);
        qDebug() << "[YOLO] 预处理图像缓冲区分配成功";

        m_initialized = true;
        qInfo() << "[YOLO] ========== 模型初始化成功 ==========";
        qInfo() << "[YOLO] 模型路径:" << modelPath;
        qInfo() << "[YOLO] 模型尺寸:" << m_config.width << "x" << m_config.height;
        qInfo() << "[YOLO] 类别数:" << m_config.numClasses;

        return true;
    }
    catch (const std::exception& e) {
        QString error = QString("初始化模型失败: %1").arg(e.what());
        qCritical() << "[YOLO EXCEPTION]" << error;
        qCritical() << "[YOLO EXCEPTION] 异常类型: std::exception";
        emit errorOccurred(error);
        
        // 清理已分配的资源
        m_inferenceEngine.reset();
        m_yolo.reset();
        m_initialized = false;
        
        return false;
    }
    catch (...) {
        QString error = "初始化模型失败: 未知异常";
        qCritical() << "[YOLO EXCEPTION]" << error;
        qCritical() << "[YOLO EXCEPTION] 异常类型: 未知";
        emit errorOccurred(error);
        
        // 清理已分配的资源
        m_inferenceEngine.reset();
        m_yolo.reset();
        m_initialized = false;
        
        return false;
    }
}

cv::Mat YoloDetector::preprocessImage(const cv::Mat& image)
{
    cv::Mat processed;
    cv::Mat imageCopy = image.clone();
    
    // 使用 demo_utils 中的预处理函数
    // PreProc(input, output, letterbox, bgr2rgb, pad_value)
    PreProc(imageCopy, processed, true, true, 114);
    
    return processed;
}

std::vector<BoundingBox> YoloDetector::detectSync(const cv::Mat& image)
{
    static int frameCount = 0;
    frameCount++;
    
    if (!m_initialized) {
        qWarning() << "[YOLO DETECT] Frame" << frameCount << ": 模型未初始化";
        return std::vector<BoundingBox>();
    }

    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[YOLO DETECT] ========== Frame" << frameCount << "==========";
    qDebug() << "[YOLO DETECT] 输入图像: " << image.cols << "x" << image.rows 
             << ", channels=" << image.channels() << ", type=" << image.type();

    try {
        // 预处理 - 创建可修改的副本
        qDebug() << "[YOLO DETECT] Step 1: 开始预处理...";
        cv::Mat imageCopy = image.clone();
        PreProc(imageCopy, m_preprocessedImage, true, true, 114);
        qDebug() << "[YOLO DETECT] Step 1: 预处理完成 -> " 
                 << m_preprocessedImage.cols << "x" << m_preprocessedImage.rows;
        
        // 同步推理
        qDebug() << "[YOLO DETECT] Step 2: 开始推理...";
        m_inferenceEngine->Run(
            m_preprocessedImage.data, 
            nullptr,  // 第二个参数设为 nullptr
            m_outputBuffer.data()  // 输出会写入这个缓冲区
        );
        
        // 获取输出张量的元数据（shape等信息）
        m_outputTensors = m_inferenceEngine->GetOutputs();
        qDebug() << "[YOLO DETECT] Step 2: 推理完成, 输出层数=" << m_outputTensors.size();
        
        // 准备 output_shape 信息
        std::vector<std::vector<int64_t>> output_shapes;
        for (size_t i = 0; i < m_outputTensors.size(); ++i) {
            auto shape = m_outputTensors[i].shape();
            output_shapes.push_back(shape);
            
            // 手動構建 shape 字符串用於調試
            QString shapeStr = "[";
            for (size_t j = 0; j < shape.size(); ++j) {
                shapeStr += QString::number(shape[j]);
                if (j < shape.size() - 1) shapeStr += ",";
            }
            shapeStr += "]";
            qDebug() << "[YOLO DETECT]   输出[" << i << "] shape:" << shapeStr;
        }
        
        // 簡單起見，直接傳入空的 data_type（PostProc 內部會根據情況處理）
        // 修復：從推理引擎獲取實際的數據類型（與 dx_app od.cpp 第70行一致）
        dxrt::DataType dataType = m_inferenceEngine->outputs().front().type();
        int outputLength = m_outputBuffer.size() / sizeof(float);
        
        qDebug() << "[YOLO DETECT] 使用 buffer 版本的 PostProc";
        qDebug() << "[YOLO DETECT]   buffer size:" << m_outputBuffer.size() << "bytes";
        qDebug() << "[YOLO DETECT]   output_shapes count:" << output_shapes.size();
        qDebug() << "[YOLO DETECT]   data type:" << static_cast<int>(dataType);

        // 后处理 - 使用 buffer 版本
        qDebug() << "[YOLO DETECT] Step 3: 开始后处理...";
        auto results = m_yolo->PostProc(m_outputBuffer.data(), output_shapes, dataType, outputLength);
        qDebug() << "[YOLO DETECT] Step 3: 后处理完成, 检测到" << results.size() << "个目标";
        
        // Step 4: 坐標轉換 - 從模型輸入尺寸縮放到原始圖像尺寸
        // 參考 dx_app-1.11.0/demos/object_detection/od.cpp GetScalingBBox
        if (results.size() > 0) {
            int srcWidth = image.cols;
            int srcHeight = image.rows;
            int npuWidth = m_config.width;   // 640
            int npuHeight = m_config.height; // 640
            
            // 計算縮放比例和padding（與PreProc的letterbox對應）
            float preprocRatio = std::min((float)npuWidth / srcWidth, (float)npuHeight / srcHeight);
            int resizedWidth = (int)(srcWidth * preprocRatio);
            int resizedHeight = (int)(srcHeight * preprocRatio);
            float padWidth = (npuWidth - resizedWidth) / 2.0f;
            float padHeight = (npuHeight - resizedHeight) / 2.0f;
            float scaleRatio = 1.0f / preprocRatio;
            
            qDebug() << "[YOLO DETECT] Step 4: 坐標轉換";
            qDebug() << "[YOLO DETECT]   原始圖像:" << srcWidth << "x" << srcHeight;
            qDebug() << "[YOLO DETECT]   預處理比例:" << preprocRatio;
            qDebug() << "[YOLO DETECT]   padding:" << padWidth << "x" << padHeight;
            qDebug() << "[YOLO DETECT]   縮放比例:" << scaleRatio;
            
            for (auto& box : results) {
                // 減去padding，然後縮放到原始圖像尺寸
                box.box[0] = (box.box[0] - padWidth) * scaleRatio;   // x1
                box.box[1] = (box.box[1] - padHeight) * scaleRatio;  // y1
                box.box[2] = (box.box[2] - padWidth) * scaleRatio;   // x2
                box.box[3] = (box.box[3] - padHeight) * scaleRatio;  // y2
            }
        }
        
        // 打印检测结果详情
        if (results.size() > 0) {
            qDebug() << "[YOLO DETECT] 检测结果详情:";
            for (size_t i = 0; i < results.size() && i < 5; i++) {  // 只打印前5个
                const auto& box = results[i];
                qDebug() << "[YOLO DETECT]   #" << i << ": label=" << box.label 
                         << "(" << QString::fromStdString(box.labelname) << ")"
                         << ", score=" << QString::number(box.score, 'f', 2)
                         << ", box=[" << box.box[0] << "," << box.box[1] << "," << box.box[2] << "," << box.box[3] << "]";
            }
            if (results.size() > 5) {
                qDebug() << "[YOLO DETECT]   ... 还有" << (results.size() - 5) << "个目标";
            }
        }
        
        qDebug() << "[YOLO DETECT] Frame" << frameCount << "处理完成 ✓";
        return results;
    }
    catch (const std::exception& e) {
        QString error = QString("推理失败: %1").arg(e.what());
        qCritical() << "[YOLO DETECT ERROR] Frame" << frameCount << ":" << error;
        emit errorOccurred(error);
        return std::vector<BoundingBox>();
    }
}

bool YoloDetector::detectAsync(const cv::Mat& image)
{
    if (!m_initialized) {
        qWarning() << "模型未初始化";
        return false;
    }

    try {
        // 预处理 - 创建可修改的副本
        cv::Mat imageCopy = image.clone();
        PreProc(imageCopy, m_preprocessedImage, true, true, 114);
        
        // 异步推理
        // 注意：这里需要确保缓冲区在回调之前保持有效
        m_inferenceEngine->RunAsync(
            m_preprocessedImage.data, 
            this,
            m_outputBuffer.data()
        );

        return true;
    }
    catch (const std::exception& e) {
        QString error = QString("异步推理启动失败: %1").arg(e.what());
        qWarning() << error;
        emit errorOccurred(error);
        return false;
    }
}

int YoloDetector::postProcessCallback(
    std::vector<std::shared_ptr<dxrt::Tensor>> outputs, 
    void* arg)
{
    auto* detector = static_cast<YoloDetector*>(arg);
    if (!detector) {
        return -1;
    }

    try {
        // 后处理
        auto results = detector->m_yolo->PostProc(outputs);
        
        // 发送信号
        emit detector->detectionComplete(results);
        
        return 0;
    }
    catch (const std::exception& e) {
        QString error = QString("后处理失败: %1").arg(e.what());
        qWarning() << error;
        emit detector->errorOccurred(error);
        return -1;
    }
}
