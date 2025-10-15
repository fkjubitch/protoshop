#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "common.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->menubar->setStyleSheet("color: rgb(208, 208, 208);");

    setWindowIcon(QIcon(":/sideBarIcons/images/rat.ico"));
    setWindowState(Qt::WindowMaximized);

    sideBarButtonGroup = new QButtonGroup();
    sideBarButtonGroup->addButton(ui->selectButton, 0);
    sideBarButtonGroup->addButton(ui->penButton, 1);
    sideBarButtonGroup->addButton(ui->lineButton, 2);
    sideBarButtonGroup->addButton(ui->rectButton, 4);
    sideBarButtonGroup->addButton(ui->polygonButton, 5);
    sideBarButtonGroup->addButton(ui->circleButton, 6);
    sideBarButtonGroup->addButton(ui->ellipseButton, 7);
    sideBarButtonGroup->addButton(ui->fillSelectButton, 8);
    sideBarButtonGroup->setExclusive(true);
    ui->selectButton->setChecked(true); // 默认选择画笔

    colorTypeButtonGroup = new QButtonGroup();
    colorTypeButtonGroup->addButton(ui->boardButton, 0);
    colorTypeButtonGroup->addButton(ui->fillButton, 1);
    colorTypeButtonGroup->setExclusive(true);
    ui->boardButton->setChecked(true);

    lineTypeButtonGroup = new QButtonGroup();
    lineTypeButtonGroup->addButton(ui->solidButton, 0);
    lineTypeButtonGroup->addButton(ui->dashButton, 1);
    lineTypeButtonGroup->addButton(ui->dotButton, 2);
    lineTypeButtonGroup->addButton(ui->dashDotButton, 3);
    lineTypeButtonGroup->setExclusive(true);
    ui->solidButton->setChecked(true);

    painterActionGroup = new QActionGroup(this);
    painterActionGroup->addAction(ui->rectSelectAction);
    painterActionGroup->addAction(ui->penAction);
    painterActionGroup->addAction(ui->lineAction);
    painterActionGroup->addAction(ui->rectAction);
    painterActionGroup->addAction(ui->polygonAction);
    painterActionGroup->addAction(ui->circleAction);
    painterActionGroup->addAction(ui->ellipseAction);
    painterActionGroup->setExclusive(true);
    ui->rectSelectAction->setChecked(true);

    colorTypeActionGroup = new QActionGroup(this);
    colorTypeActionGroup->addAction(ui->boardColorAction);
    colorTypeActionGroup->addAction(ui->fillColorAction);
    colorTypeActionGroup->setExclusive(true);
    ui->boardColorAction->setChecked(true);

    lineTypeActionGroup = new QActionGroup(this);
    lineTypeActionGroup->addAction(ui->solidAction);
    lineTypeActionGroup->addAction(ui->dashAction);
    lineTypeActionGroup->addAction(ui->dotAction);
    lineTypeActionGroup->addAction(ui->dashDotAction);
    lineTypeActionGroup->setExclusive(true);
    ui->solidAction->setChecked(true);

    // 创建场景
    m_scene = new QGraphicsScene(ui->graphicsView);
    m_scene->setSceneRect(ui->graphicsView->sceneRect());
    ui->graphicsView->setScene(m_scene);

    // 连接信号与槽
    connect(ui->graphicsView, &CustomView::sendMousePos, this, &MainWindow::receiveMousePos);
    connect(ui->palatteButton, &QPushButton::clicked, ui->graphicsView, &CustomView::palatteButtonClicked);
    connect(ui->delete_action, &QAction::triggered, ui->graphicsView, &CustomView::onDeleteActionClicked);
    connect(ui->saveAction, &QAction::triggered, ui->graphicsView, &CustomView::onSaveAs);
    connect(ui->openAction, &QAction::triggered, ui->graphicsView, &CustomView::onOpen);
    connect(ui->exitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui->revokeAction, &QAction::triggered, ui->graphicsView, &CustomView::onRevoke);
    connect(ui->undoAction, &QAction::triggered, ui->graphicsView, &CustomView::onUndo);
    connect(ui->rectSelectAction, &QAction::triggered, ui->selectButton, &QPushButton::click);
    connect(ui->penAction, &QAction::triggered, ui->penButton, &QPushButton::click);
    connect(ui->lineAction, &QAction::triggered, ui->lineButton, &QPushButton::click);
    connect(ui->rectAction, &QAction::triggered, ui->rectButton, &QPushButton::click);
    connect(ui->polygonAction, &QAction::triggered, ui->polygonButton, &QPushButton::click);
    connect(ui->circleAction, &QAction::triggered, ui->circleButton, &QPushButton::click);
    connect(ui->ellipseAction, &QAction::triggered, ui->ellipseButton, &QPushButton::click);
    connect(ui->fillSelectAction, &QAction::triggered, ui->fillSelectButton, &QPushButton::click);
    connect(ui->boardColorAction, &QAction::triggered, ui->boardButton, &QPushButton::click);
    connect(ui->fillColorAction, &QAction::triggered, ui->fillButton, &QPushButton::click);
    connect(ui->solidAction, &QAction::triggered, ui->solidButton, &QPushButton::toggle);
    connect(ui->dashAction, &QAction::triggered, ui->dashButton, &QPushButton::toggle);
    connect(ui->dotAction, &QAction::triggered, ui->dotButton, &QPushButton::toggle);
    connect(ui->dashDotAction, &QAction::triggered, ui->dashDotButton, &QPushButton::toggle);
    connect(ui->palatteAction, &QAction::triggered, ui->palatteButton, &QPushButton::click);
    connect(ui->hotkeysHelp, &QAction::triggered, this, &MainWindow::onHelpTriggered);
    connect(ui->about, &QAction::triggered, this, &MainWindow::onAboutTriggered);
    connect(ui->widthAction, &QAction::triggered, this, &MainWindow::onWidthAction);
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
        ui->penAction->setChecked(true);
    }
}


void MainWindow::on_lineButton_clicked()
{
    if (ui->lineButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::LINE);
        ui->lineAction->setChecked(true);
    }
}


void MainWindow::on_rectButton_clicked()
{
    if (ui->rectButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::RECT);
        ui->rectAction->setChecked(true);
    }
}


void MainWindow::on_polygonButton_clicked()
{
    if (ui->polygonButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::POLYGON);
        ui->polygonAction->setChecked(true);
    }
}


void MainWindow::on_circleButton_clicked()
{
    if (ui->circleButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::CIRCLE);
        ui->circleAction->setChecked(true);
    }
}


void MainWindow::on_ellipseButton_clicked()
{
    if (ui->ellipseButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::ELLIPSE);
        ui->ellipseAction->setChecked(true);
    }
}


void MainWindow::on_selectButton_clicked()
{
    if (ui->selectButton->isChecked())
    {
        ui->graphicsView->setPainterStatus(PainterStatus::SELECT);
        ui->rectSelectAction->setChecked(true);
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


void MainWindow::on_solidButton_toggled(bool checked)
{
    if(checked){
        ui->graphicsView->penStyle = Qt::SolidLine;
        ui->solidAction->setChecked(true);
    }
}


void MainWindow::on_dashButton_toggled(bool checked)
{
    if(checked){
        ui->graphicsView->penStyle = Qt::DashLine;
        ui->dashAction->setChecked(true);
    }
}


void MainWindow::on_dotButton_toggled(bool checked)
{
    if(checked){
        ui->graphicsView->penStyle = Qt::DotLine;
        ui->dotAction->setChecked(true);
    }
}


void MainWindow::on_dashDotButton_toggled(bool checked)
{
    if(checked){
        ui->graphicsView->penStyle = Qt::DashDotLine;
        ui->dashDotAction->setChecked(true);
    }
}


void MainWindow::on_boardButton_clicked()
{
    if(ui->boardButton->isChecked())
    {
        ui->graphicsView->colorType = ColorType::BOARD;
        ui->boardColorAction->setChecked(true);
    }
}


void MainWindow::on_fillButton_clicked()
{
    if(ui->fillButton->isChecked())
    {
        ui->graphicsView->colorType = ColorType::FILL;
        ui->fillColorAction->setChecked(true);
    }
}

void MainWindow::onHelpTriggered()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("快捷键帮助"));
    box.setTextFormat(Qt::RichText);
    box.setText(
        tr("<h3 style=\"text-align: center\">快捷键</h3>"
           "<p><b>Delete</b> – 删除选中图元<br/>"
           "<b>Ctrl+Z</b> – 撤销<br/>"
           "<b>Ctrl+Y</b> – 重做<br/>"
           "<b>Ctrl+S</b> – 保存为 PNG 或 Json<br/>"
           "<b>鼠标左键</b> – 绘制/选中/缩放/旋转/调节节点<br/>"
           "<b>鼠标右键</b> – 结束多边形</p>"
           "<p>暂不支持自定义快捷键。</p>"));
    box.setStandardButtons(QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Ok);

    box.setStyleSheet(
        "QMessageBox { color: white; background-color: rgb(30, 30, 30); }"
        "QLabel { color: white; }"
        "QPushButton { color: white; }"
        );

    box.exec();
}


void MainWindow::onAboutTriggered()
{
    // 1. 对话框本体
    QDialog dlg(this);
    dlg.setWindowTitle(tr("关于"));
    dlg.setFixedSize(400, 220);
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // 2. 图标
    QLabel *iconLbl = new QLabel;
    QPixmap pix(":/sideBarIcons/images/arat.jpg");   // 换成自己的资源或文件路径
    if (!pix.isNull())
        iconLbl->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 3. 右侧文字
    QLabel *textLbl = new QLabel;
    textLbl->setOpenExternalLinks(true);   // 允许点击超链接
    textLbl->setText(
        QString(R"(<h3>Protoshop - 简易绘图程序</h3>
                   <p>version: beta<br/>
                   author: 吴逢昊<br/>
                   copyright © 2025 一只白鼠</p>
                   <p>GitHub:
                   <a href="https://github.com/fkjubitch/protoshop">https://github.com/fkjubitch/protoshop</a></p>
                   <p>喜欢的话点个Star吧。<br/>由于能力有限, Protoshop 仍存在各种问题, <br/>期待各位大佬一起帮助改进和完善。</p>)"));

    // 4. 顶部水平布局（图标 + 文字）
    QHBoxLayout *topLay = new QHBoxLayout;
    topLay->addWidget(iconLbl);
    topLay->addWidget(textLbl, 1);
    topLay->addStretch();

    // 5. 底部确定按钮
    QPushButton *okBtn = new QPushButton(tr("确定"));
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QHBoxLayout *btnLay = new QHBoxLayout;
    btnLay->addStretch();
    btnLay->addWidget(okBtn);

    // 6. 总布局
    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);
    mainLay->addLayout(topLay);
    mainLay->addStretch();
    mainLay->addLayout(btnLay);
    dlg.setStyleSheet(
        "QInputDialog { color: white; background-color: rgb(30, 30, 30); }"
        "QLabel { color: white; }"
        "QSpinBox { color: white; }"
        "QPushButton { color: white; }"
        );

    dlg.exec();
}

void MainWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->modifiers() == Qt::ControlModifier) {
        switch (ev->key()) {
        case Qt::Key_Z:
            keyCtrlZ();
            return;
        case Qt::Key_Y:
            keyCtrlY();
            return;
        case Qt::Key_S:
            keyCtrlS();
            return;
        }
    }
    QMainWindow::keyPressEvent(ev);
}

void MainWindow::keyCtrlZ()
{
    ui->graphicsView->onRevoke();
}

void MainWindow::keyCtrlY()
{
    ui->graphicsView->onUndo();
}

void MainWindow::keyCtrlS()
{
    ui->graphicsView->onSaveAs();
}

void MainWindow::onWidthAction()
{
    QInputDialog dlg(this);
    dlg.setWindowTitle(tr("线条宽度设置"));
    dlg.setLabelText(tr("请输入线宽(1~50):"));
    dlg.setIntRange(1, 50);
    dlg.setIntValue(ui->graphicsView->penWidth);
    dlg.setOkButtonText(tr("确定"));
    dlg.setCancelButtonText(tr("取消"));

    dlg.setStyleSheet(
        "QInputDialog { color: white; background-color: rgb(30, 30, 30); }"
        "QLabel { color: white; }"
        "QSpinBox { color: white; }"
        "QPushButton { color: white; }"
        );

    if (dlg.exec() == QDialog::Accepted) {
        int w = dlg.intValue();
        ui->graphicsView->penWidth = w;
        ui->spinBox->setValue(w);
    }
}

void MainWindow::on_fillSelectButton_clicked()
{
    if(ui->fillSelectButton->isChecked()){
        ui->graphicsView->setPainterStatus(PainterStatus::FILLSELECT);
        ui->fillSelectButton->setChecked(true);
    }
}

