/**
 * @file main.cpp
 * @brief 程序入口（Windows-only 上位机）
 *
 * ✅ 需求对齐：
 * - Qt Widgets 程序
 * - 全部配置（颜色表/冲突表/设备属性/快捷键/串口/最近Excel路径）由 AppSettings(QSettings/ini) 管理
 * - UI：MainWindow（两页签：状态/设置）
 */

#include <QApplication>
#include <QCoreApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 组织/应用名用于 QSettings 的默认作用域（我们后续会指定 ini 路径，仍建议保留）
    QCoreApplication::setOrganizationName("VisualLab");
    QCoreApplication::setOrganizationDomain("visual.lab.local");
    QCoreApplication::setApplicationName("VisualWorkflowHost");

    MainWindow w;
    w.show();

    return app.exec();
}
