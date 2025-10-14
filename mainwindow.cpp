#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    sideBarButtonGroup = new QButtonGroup();
    sideBarButtonGroup->addButton(ui->selectButton, 0);
    sideBarButtonGroup->addButton(ui->penButton, 1);
    sideBarButtonGroup->addButton(ui->lineButton, 2);
    sideBarButtonGroup->addButton(ui->curveButton, 3);
    sideBarButtonGroup->addButton(ui->rectButton, 4);
    sideBarButtonGroup->addButton(ui->polygonButton, 5);
    sideBarButtonGroup->addButton(ui->circleButton, 6);
    sideBarButtonGroup->addButton(ui->ellipseButton, 7);
    sideBarButtonGroup->setExclusive(true);
    ui->selectButton->setChecked(true); // 默认选择画笔

    colorTypeButtonGroup = new QButtonGroup();
    colorTypeButtonGroup->addButton(ui->boardButton, 0);
    colorTypeButtonGroup->addButton(ui->fillButton, 1);
    colorTypeButtonGroup->setExclusive(true);
    ui->boardButton->setChecked(true);

    // 创建场景
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(ui->graphicsView->rect());
    ui->graphicsView->setScene(m_scene);

    // 连接信号与槽
    connect(ui->graphicsView, &CustomView::sendMousePos, this, &MainWindow::receiveMousePos);
    connect(ui->palatteButton, &QPushButton::clicked, ui->graphicsView, &CustomView::palatteButtonClicked);
    connect(ui->boardButton, &QPushButton::toggled, ui->graphicsView, &CustomView::boardButtonChecked);
    connect(ui->fillButton, &QPushButton::toggled, ui->graphicsView, &CustomView::fillButtonChecked);
    connect(ui->delete_action, &QAction::triggered, ui->graphicsView, &CustomView::onDeleteActionClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_penButton_clicked()
{
    if (ui->penButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::PEN);
    }
}


void MainWindow::on_lineButton_clicked()
{
    if (ui->lineButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::LINE);
    }
}


void MainWindow::on_curveButton_clicked()
{
    if (ui->curveButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::CURVE);
    }
}


void MainWindow::on_rectButton_clicked()
{
    if (ui->rectButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::RECT);
    }
}


void MainWindow::on_polygonButton_clicked()
{
    if (ui->polygonButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::POLYGON);
    }
}


void MainWindow::on_circleButton_clicked()
{
    if (ui->circleButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::CIRCLE);
    }
}


void MainWindow::on_ellipseButton_clicked()
{
    if (ui->ellipseButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::ELLIPSE);
    }
}


void MainWindow::on_selectButton_clicked()
{
    if (ui->selectButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::SELECT);
    }
}

void MainWindow::receiveMousePos(QPointF pos)
{
    QString cordInfo;
    cordInfo += "X: ";
    cordInfo += QString::number(pos.x());
    cordInfo += " Y: ";
    cordInfo += QString::number(pos.y());
    ui->cord->setText(cordInfo);
}


void MainWindow::on_spinBox_valueChanged(int arg1)
{
    ui->graphicsView->penWidth = arg1;
}

