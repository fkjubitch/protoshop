#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QGraphicsScene>

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

    void on_curveButton_clicked();

    void on_rectButton_clicked();

    void on_polygonButton_clicked();

    void on_circleButton_clicked();

    void on_ellipseButton_clicked();

    void on_selectButton_clicked();

    void receiveMousePos(QPointF pos);

private:
    Ui::MainWindow *ui;
    QButtonGroup* sideBarButtonGroup;
    QGraphicsScene* m_scene = nullptr;
};
#endif // MAINWINDOW_H
