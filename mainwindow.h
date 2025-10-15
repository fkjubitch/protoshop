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

private slots:
    void on_penButton_clicked();

    void on_lineButton_clicked();

    void on_rectButton_clicked();

    void on_polygonButton_clicked();

    void on_circleButton_clicked();

    void on_ellipseButton_clicked();

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

private:
    QButtonGroup* sideBarButtonGroup = nullptr;
    QButtonGroup* colorTypeButtonGroup = nullptr; // 着色类型按钮组
    QButtonGroup* lineTypeButtonGroup = nullptr; // 线框样式按钮组
    QActionGroup* painterActionGroup = nullptr;
    QActionGroup* colorTypeActionGroup = nullptr;
    QActionGroup* lineTypeActionGroup = nullptr;
    QGraphicsScene* m_scene = nullptr;

public:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
