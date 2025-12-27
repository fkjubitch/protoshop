#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QGraphicsScene>
#include <QColorDialog>
#include <QActionGroup>
#include <QMessageBox>
#include <QPixmap>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMenu>
#include "common.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // 算法选择菜单
    QMenu* m_lineAlgorithmMenu = nullptr;    // 直线算法菜单
    QMenu* m_circleAlgorithmMenu = nullptr;  // 圆形算法菜单
    QMenu* m_ellipseAlgorithmMenu = nullptr; // 椭圆算法菜单
    QMenu* m_curveMenu = nullptr;             // 曲线子菜单（贝塞尔 / B样条）

    void createRasterAlgorithmMenus();  // 创建光栅算法菜单
    void removeRasterAlgorithmMenus();  // 移除光栅算法菜单
    void createCurveMenu();
    void handleToolButtonCheck(QPushButton* button, PainterStatus status);

private slots:
    void on_penButton_clicked();

    void on_lineButton_clicked(bool checked = false);

    void on_rectButton_clicked();

    void on_polygonButton_clicked();

    void on_circleButton_clicked(bool checked = false);

    void on_ellipseButton_clicked(bool checked = false);

    void on_selectButton_clicked();

    void receiveMousePos(QPointF pos);

    void on_spinBox_valueChanged(int arg1);

    void on_solidButton_toggled(bool checked);

    void on_dashButton_toggled(bool checked);

    void on_dotButton_toggled(bool checked);

    void on_dashDotButton_toggled(bool checked);

    void on_boardButton_clicked();

    void on_fillButton_clicked();

    void onHelpTriggered();

    void onAboutTriggered();

    void keyCtrlZ();   // Ctrl+Z 撤销
    void keyCtrlY();   // Ctrl+Y 重做
    void keyCtrlS();   // Ctrl+S 保存

    void keyPressEvent(QKeyEvent *ev) override;

    void onWidthAction();

    void on_fillSelectButton_clicked();

    void on_rasterActionChecked(bool checked);

    void on_libActionChecked(bool checked);

    // 算法选择槽函数
    void onLineAlgorithmTriggered(QAction* action);
    void onCircleAlgorithmTriggered(QAction* action);
    void onEllipseAlgorithmTriggered(QAction* action);

    void on_curveButton_clicked();

    void on_curveSurfaceButton_clicked();

private:
    QButtonGroup* sideBarButtonGroup = nullptr;
    QButtonGroup* colorTypeButtonGroup = nullptr; // 着色类型按钮组
    QButtonGroup* lineTypeButtonGroup = nullptr; // 线框样式按钮组
    QActionGroup* painterActionGroup = nullptr;
    QActionGroup* colorTypeActionGroup = nullptr;
    QActionGroup* lineTypeActionGroup = nullptr;
    QActionGroup* implMethodActionGroup = nullptr; // 实现方式选择组
    QGraphicsScene* m_scene = nullptr;

    // 子菜单样式
    QString menuStyle = R"(
    QMenu {
        background-color: rgb(101, 102, 104);  /* 菜单背景色，与按钮一致 */
        border: 1px solid rgb(80, 80, 80);
    }
    QMenu::item {
        color: white;              /* 正常状态文字颜色 */
        padding: 5px 20px 5px 20px;
        background-color: transparent;
    }
    QMenu::item:selected {         /* 鼠标悬停/选中状态 */
        background-color: rgb(120, 120, 120);
        color: white;
    }
    QMenu::item:disabled {         /* 禁用状态 */
        color: rgb(160, 160, 160);
    }
    QMenu::separator {             /* 分隔线 */
        height: 1px;
        background-color: rgb(80, 80, 80);
    }
)";

public:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
