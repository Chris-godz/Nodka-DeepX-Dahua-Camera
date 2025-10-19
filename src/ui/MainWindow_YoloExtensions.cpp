#include "MainWindow.h"
#include <QDebug>
#include <QThread>

// YOLO 检测结果回调 - 无参数版本，主动获取数据
void MainWindow::onDetectionsUpdated()
{
    static int updateCount = 0;
    updateCount++;
    
    qDebug() << "[UI] ========== 进入 onDetectionsUpdated #" << updateCount << " ==========";
    qDebug() << "[UI] 当前线程 ID:" << QThread::currentThreadId();
    
    // 从 CameraController 主动获取最新检测结果
    auto detections = m_cameraController->getLatestDetections();
    
    qDebug() << "[UI] 检测目标数量:" << detections.size();
    qDebug() << "[UI] 开始复制到 m_latestDetections...";
    m_latestDetections = detections;
    qDebug() << "[UI] 复制完成 ✓";
    
    if (detections.size() > 0) {
        qDebug() << "[UI] 目标详情:";
        for (size_t i = 0; i < detections.size() && i < 3; i++) {  // 只打印前3个
            const auto& box = detections[i];
            qDebug() << "[UI]   #" << i << ": label=" << box.label 
                     << "(" << QString::fromStdString(box.labelname) << ")"
                     << ", score=" << QString::number(box.score, 'f', 2)
                     << ", box=[" << box.box[0] << "," << box.box[1] << "," << box.box[2] << "," << box.box[3] << "]";
        }
        if (detections.size() > 3) {
            qDebug() << "[UI]   ... 还有" << (detections.size() - 3) << "个目标";
        }
    }
}

// 将 cv::Mat 转换为 QImage
QImage MainWindow::cvMatToQImage(const cv::Mat& mat)
{
    switch (mat.type())
    {
    case CV_8UC1:
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    case CV_8UC3:
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_RGB888).rgbSwapped().copy();
    case CV_8UC4:
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_ARGB32).copy();
    default:
        qWarning() << "不支持的图像格式";
        return QImage();
    }
}

// 在图像上绘制检测框
void MainWindow::drawDetectionsOnImage(cv::Mat& image, const std::vector<BoundingBox>& detections)
{
    static int drawCount = 0;
    drawCount++;
    
    if (image.empty()) {
        qWarning() << "[UI DRAW] 图像为空，无法绘制";
        return;
    }
    
    if (detections.empty())
    {
        if (drawCount % 50 == 0) {  // 每50次打印一次
            qDebug() << "[UI DRAW] 没有检测结果需要绘制 (已绘制" << drawCount << "次)";
        }
        return;
    }
    
    qDebug() << "[UI DRAW] ========== 绘制第" << drawCount << "帧 ==========";
    qDebug() << "[UI DRAW] 图像尺寸:" << image.cols << "x" << image.rows;
    qDebug() << "[UI DRAW] 绘制" << detections.size() << "个检测框";
    
    // COCO dataset class names
    static const std::vector<std::string> classNames = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
        "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
        "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
        "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
        "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
        "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
        "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
        "toothbrush"
    };
    
    // 定义一些颜色用于不同类别
    std::vector<cv::Scalar> colors = {
        cv::Scalar(255, 0, 0),    // 蓝色
        cv::Scalar(0, 255, 0),    // 绿色
        cv::Scalar(0, 0, 255),    // 红色
        cv::Scalar(255, 255, 0),  // 青色
        cv::Scalar(255, 0, 255),  // 品红
        cv::Scalar(0, 255, 255),  // 黄色
    };
    
    int drawnBoxes = 0;
    for (const auto& box : detections)
    {
        // BoundingBox结构: box[0]=x1, box[1]=y1, box[2]=x2, box[3]=y2 (左上角和右下角坐標)
        int x1 = static_cast<int>(box.box[0]);
        int y1 = static_cast<int>(box.box[1]);
        int x2 = static_cast<int>(box.box[2]);
        int y2 = static_cast<int>(box.box[3]);
        
        // 选择颜色
        cv::Scalar color = colors[box.label % colors.size()];
        
        // 绘制边界框（直接使用 x1,y1 到 x2,y2）
        cv::rectangle(image, 
                     cv::Point(x1, y1),
                     cv::Point(x2, y2),
                     color, 2);
        
        // 准备标签文本
        std::string className;
        if (!box.labelname.empty())
        {
            className = box.labelname;
        }
        else if (box.label >= 0 && box.label < static_cast<int>(classNames.size()))
        {
            className = classNames[box.label];
        }
        else
        {
            className = "Object";
        }
        
        std::string label = className + ": " + std::to_string(static_cast<int>(box.score * 100)) + "%";
        
        // 获取文本大小
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        
        // 绘制标签背景
        cv::rectangle(image,
                     cv::Point(x1, y1 - textSize.height - 5),
                     cv::Point(x1 + textSize.width, y1),
                     color, cv::FILLED);
        
        // 绘制标签文本
        cv::putText(image, label,
                   cv::Point(x1, y1 - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5,
                   cv::Scalar(255, 255, 255), 1);
        
        drawnBoxes++;
    }
    
    qDebug() << "[UI DRAW] 成功绘制" << drawnBoxes << "个检测框 ✓";
}
