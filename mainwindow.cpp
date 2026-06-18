#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtNetwork/QNetworkInterface>
#include <QList>
#include <QComboBox>
#include <QDebug>
#include <QMessageBox>
#include <QValidator>
#include <QRegularExpression>
#include <QSettings>
#include <QWindow>
#include "constants.h"
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QProcess>
#include <QTimer>
#include <QStyle>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QStyleHints>
#include <QShowEvent>
#include "macoshelper.h"

#include "encrypt/EncryptData.h"

MainWindow::MainWindow(SingleApplication *parentApp, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow),
    app(parentApp)
{
    // 关机时接收退出信号，释放socket
    QObject::connect(app, &SingleApplication::aboutToQuit, this, &MainWindow::QuitDrcom);

    qDebug()<<"MainWindow constructor";
	ui->setupUi(this);
	setWindowFlags((windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint)
		& ~Qt::WindowContextHelpButtonHint);
	setAttribute(Qt::WA_TranslucentBackground);
	InstallMacLiquidGlass(reinterpret_cast<void *>(winId()));
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
		this, &MainWindow::ApplyTheme);
	ApplyTheme(QGuiApplication::styleHints()->colorScheme());
	if (qEnvironmentVariableIsSet("DRCOM_PREVIEW_DARK")) {
		ApplyTheme(Qt::ColorScheme::Dark);
	}
	setWindowIcon(QIcon(":/images/AppIcon.png"));
	passwordVisibilityAction = ui->lineEditPass->addAction(QIcon(":/images/eye.svg"), QLineEdit::TrailingPosition);
	passwordVisibilityAction->setToolTip(tr("显示密码"));
	connect(passwordVisibilityAction, &QAction::triggered, this, &MainWindow::on_buttonTogglePassword_clicked);
	ui->lineEditAccount->setFocus();
	SetConnectionStatus(tr("未连接"), tr("填写校园网账号后开始认证"), "#98a2b3");
	if (qEnvironmentVariableIsSet("DRCOM_PREVIEW_ONLINE")) {
		SetConnectionStatus(tr("已连接"), tr("校园网认证成功，连接保持中"), "#18a96f");
		ui->labelIp->setText("IP：10.100.0.8");
	}
	setWindowOpacity(0.0);
	QTimer::singleShot(0, this, [this]() {
		auto *animation = new QPropertyAnimation(this, "windowOpacity", this);
		animation->setDuration(180);
		animation->setStartValue(0.0);
		animation->setEndValue(1.0);
		animation->setEasingCurve(QEasingCurve::OutCubic);
		animation->start(QAbstractAnimation::DeleteWhenStopped);
	});

	CURR_STATE = STATE_OFFLINE;

	// 记住窗口大小功能
	QSettings settings;
	restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
	resize(480, 620);
	connect(ui->lineEditMAC, &QLineEdit::editingFinished, this, [this]() {
		if (ui->lineEditMAC->hasAcceptableInput()) {
			QSettings savedSettings(SETTINGS_FILE_NAME);
			savedSettings.setValue(ID_MAC, ui->lineEditMAC->text().toUpper());
		}
	});

	restartAction = new QAction(tr("重新启动 JLU Link"), this);
	connect(restartAction, &QAction::triggered, this, &MainWindow::RestartDrcomByUser);
	restoreAction = new QAction(tr("隐藏主窗口"), this);
	connect(restoreAction, &QAction::triggered, this, [this]() {
		if (isVisible() && !isMinimized()) {
			HideToMenuBar();
		} else {
			ShowLoginWindow();
		}
	});
	logOutAction = new QAction(tr("连接校园网"), this);
	connect(logOutAction, &QAction::triggered, this, [this]() {
		if (CURR_STATE == STATE_ONLINE) {
			UserLogOut();
		} else if (CURR_STATE == STATE_OFFLINE) {
			ShowLoginWindow();
			ui->pushButtonLogin->click();
		}
	});
	aboutAction = new QAction(tr("关于 JLU Link"), this);
	connect(aboutAction, &QAction::triggered, this, &MainWindow::AboutDrcom);
	quitAction = new QAction(tr("退出 JLU Link"), this);
	connect(quitAction, &QAction::triggered, this, &MainWindow::QuitDrcom);
	statusAction = new QAction(tr("状态：未连接"), this);
	statusAction->setEnabled(false);

	trayIconMenu = new QMenu(this);
	trayIconMenu->addAction(statusAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(restoreAction);
	trayIconMenu->addAction(logOutAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(restartAction);
	trayIconMenu->addAction(aboutAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(quitAction);

	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setContextMenu(trayIconMenu);
	connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::IconActivated);
	SetIcon(false);
	trayIcon->show();

	windowMenu = new QMenu(tr("帮助"), this);
	windowMenu->addAction(aboutAction);
	ui->menuBar->addMenu(windowMenu);

	// 读取配置文件
	LoadSettings();

	// 设置回调函数
	dogcomController = new DogcomController();
	connect(dogcomController, &DogcomController::HaveBeenOffline,
		this, &MainWindow::HandleOffline);
	connect(dogcomController, &DogcomController::HaveLoggedIn,
		this, &MainWindow::HandleLoggedIn);
	connect(dogcomController, &DogcomController::HaveObtainedIp,
		this, &MainWindow::HandleIpAddress);

	// 验证手动输入的mac地址
    macValidator = new QRegularExpressionValidator(QRegularExpression("[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}"), this);
	ui->lineEditMAC->setValidator(macValidator);

	// 尚未登录 不可注销
	UpdateTrayMenu();

	// 自动登录功能
	QSettings s(SETTINGS_FILE_NAME);
	int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
	qDebug() << "MainWindow constructor: restartTimes=" << restartTimes;
	if (qEnvironmentVariableIsSet("DRCOM_SCREENSHOT_PATH")) {
		return;
	}
	if (restartTimes == 0) {
        if (bAutoLogin) {
			emit ui->pushButtonLogin->click();
		}
	}
	else {
		// 尝试自动重启中
		emit ui->pushButtonLogin->click();
	}
}

void MainWindow::AboutDrcom() {
	QDesktopServices::openUrl(QUrl("https://github.com/Dotoiia/JLU-Link"));
}

void MainWindow::closeEvent(QCloseEvent *event) {
	QSettings settings;
	settings.setValue("mainWindowGeometry", saveGeometry());
	if (bQuit || bRestart) {
		event->accept();
		return;
	}
	event->ignore();
	HideToMenuBar();
}

void MainWindow::showEvent(QShowEvent *event) {
	QDialog::showEvent(event);
	if (restoreAction) restoreAction->setText(tr("隐藏主窗口"));
}

void MainWindow::changeEvent(QEvent *event) {
	QDialog::changeEvent(event);
	if (event->type() == QEvent::WindowStateChange && restoreAction) {
		restoreAction->setText(isMinimized() ? tr("打开主窗口") : tr("隐藏主窗口"));
	}
}

void MainWindow::ShowLoginWindow() {
	SetMacApplicationVisibleInDock(true);
	if (!isVisible() || isMinimized()) {
		setWindowOpacity(0.0);
		showNormal();
		auto *animation = new QPropertyAnimation(this, "windowOpacity", this);
		animation->setDuration(180);
		animation->setStartValue(0.0);
		animation->setEndValue(1.0);
		animation->setEasingCurve(QEasingCurve::OutCubic);
		animation->start(QAbstractAnimation::DeleteWhenStopped);
	}
	restoreAction->setText(tr("隐藏主窗口"));
	raise();
	activateWindow();
}

void MainWindow::HideToMenuBar()
{
	if (!isVisible()) return;
	auto *animation = new QPropertyAnimation(this, "windowOpacity", this);
	animation->setDuration(140);
	animation->setStartValue(windowOpacity());
	animation->setEndValue(0.0);
	animation->setEasingCurve(QEasingCurve::InCubic);
	connect(animation, &QPropertyAnimation::finished, this, [this]() {
		hide();
		setWindowOpacity(1.0);
		SetMacApplicationVisibleInDock(false);
		restoreAction->setText(tr("打开主窗口"));
	});
	animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::RestartDrcom()
{
    bRestart=true;
    if(CURR_STATE==STATE_ONLINE)
        dogcomController->LogOut();
    else if(CURR_STATE==STATE_OFFLINE){
        qDebug() << "quiting current instance...";
        qApp->quit();
        qDebug() << "Restarting Drcom...";
        QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
        qDebug() << "Restart done.";
    }
    // else if(CURR_STATE==STATE_LOGGING)
    // 正在登录时候退出，假装没看到，不理
}

void MainWindow::QuitDrcom()
{
	// 退出之前恢复重试计数
    QSettings s(SETTINGS_FILE_NAME);
    s.setValue(ID_RESTART_TIMES, 0);
	qDebug() << "reset restartTimes";
	qDebug() << "QuitDrcom";

    // TODO : release socket
    bQuit=true;
    if(CURR_STATE==STATE_ONLINE)
        dogcomController->LogOut();
    else if(CURR_STATE==STATE_OFFLINE)
        // qApp->quit();
        QTimer::singleShot(0, qApp, SLOT(quit()));
    // else if(CURR_STATE==STATE_LOGGING)
    // 正在登录时候退出，假装没看到，不理

    // qApp->quit()调用放到了注销响应那块
}

void MainWindow::RestartDrcomByUser()
{
	// 重启之前恢复重试计数
	QSettings s(SETTINGS_FILE_NAME);
	s.setValue(ID_RESTART_TIMES, 0);
	qDebug() << "reset restartTimes";
	RestartDrcom();
}

void MainWindow::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason) {
	case QSystemTrayIcon::DoubleClick: {
		restoreAction->activate(restoreAction->Trigger);
		break;
	}
	default:
		break;
	}
}

void MainWindow::LoadSettings() {
	QSettings s(SETTINGS_FILE_NAME);
	account = s.value(ID_ACCOUNT, "").toString();
	mac_addr = s.value(ID_MAC, "").toString();
    bRemember = s.value(ID_REMEMBER, false).toBool();
    bAutoLogin = s.value(ID_AUTO_LOGIN, false).toBool();
    bAutoReconnect = s.value(ID_AUTO_RECONNECT, false).toBool();
    bHideWindow = s.value(ID_HIDE_WINDOW, false).toBool();
    bNotShowWelcome = s.value(ID_NOT_SHOW_WELCOME, false).toBool();
	ui->lineEditAccount->setText(account);
	SetMAC(mac_addr);
    if (bRemember)
		ui->checkBoxRemember->setCheckState(Qt::CheckState::Checked);
	else
		ui->checkBoxRemember->setCheckState(Qt::CheckState::Unchecked);
    if (bAutoLogin)
		ui->checkBoxAutoLogin->setCheckState(Qt::CheckState::Checked);
	else
		ui->checkBoxAutoLogin->setCheckState(Qt::CheckState::Unchecked);
    ui->checkBoxAutoReconnect->setChecked(bAutoReconnect);
    if(bHideWindow)
        ui->checkBoxHideLoginWindow->setCheckState(Qt::CheckState::Checked);
    else
        ui->checkBoxHideLoginWindow->setCheckState(Qt::CheckState::Unchecked);
    if(bNotShowWelcome)
        ui->checkBoxNotShowWelcome->setCheckState(Qt::CheckState::Checked);
    else
        ui->checkBoxNotShowWelcome->setCheckState(Qt::CheckState::Unchecked);

    // 密码加密处理
    QString encryptedPasswd = s.value(ID_PASSWORD, "").toString();
    password = DecryptString(encryptedPasswd);
    ui->lineEditPass->setText(password);
}

void MainWindow::SaveSettings() {
	QSettings s(SETTINGS_FILE_NAME);
	s.setValue(ID_ACCOUNT, account);
	s.setValue(ID_MAC, mac_addr);
    s.setValue(ID_REMEMBER, bRemember);
    s.setValue(ID_AUTO_LOGIN, bAutoLogin);
    s.setValue(ID_AUTO_RECONNECT, bAutoReconnect);

    // 密码加密处理
    QString enctyptedPasswd = EncryptString(password);
    s.setValue(ID_PASSWORD, enctyptedPasswd);
}

void MainWindow::SetMAC(const QString &m)
{
	QString selectedMac = m.trimmed().toUpper();
	if (selectedMac.isEmpty()) {
		foreach (const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
			const QString candidate = interface.hardwareAddress().toUpper();
			const auto flags = interface.flags();
			if (!flags.testFlag(QNetworkInterface::IsLoopBack)
				&& flags.testFlag(QNetworkInterface::IsUp)
				&& candidate.length() == 17) {
				selectedMac = candidate;
				break;
			}
		}
	}
	ui->lineEditMAC->setText(selectedMac);
}

MainWindow::~MainWindow()
{
    qDebug()<<"MainWindow destructor";
	delete ui;
    delete restartAction;
    delete restoreAction;
    delete logOutAction;
    delete quitAction;
    delete statusAction;
    delete trayIconMenu;
    delete trayIcon;
    delete aboutAction;
    delete windowMenu;
    delete dogcomController;
    delete macValidator;
}

void MainWindow::on_checkBoxAutoLogin_toggled(bool checked)
{
	if (checked) {
		ui->checkBoxRemember->setChecked(true);
	}
}

void MainWindow::on_checkBoxRemember_toggled(bool checked)
{
	if (!checked) {
		ui->checkBoxAutoLogin->setChecked(false);
		ui->checkBoxAutoReconnect->setChecked(false);
	}
}

void MainWindow::on_checkBoxAutoReconnect_toggled(bool checked)
{
	bAutoReconnect = checked;
	if (checked) ui->checkBoxRemember->setChecked(true);
	QSettings s(SETTINGS_FILE_NAME);
	s.setValue(ID_AUTO_RECONNECT, checked);
}

void MainWindow::on_pushButtonLogin_clicked()
{
	if (CURR_STATE == STATE_ONLINE) {
		UserLogOut();
		return;
	}
	GetInputs();
	if (!account.compare("")
		|| !password.compare("")
        || !mac_addr.compare("")) {
		QMessageBox::warning(this, APP_NAME, tr("Input can not be empty!"));
		return;
	}
	if (!ui->lineEditMAC->hasAcceptableInput()) {
		QMessageBox::warning(this, APP_NAME, tr("Illegal MAC address!"));
		return;
	}
	// 输入无误，执行登录操作
	// 先禁用输入框和按钮
	SetDisableInput(true);
	CURR_STATE = STATE_LOGGING;
	UpdateTrayMenu();
	ui->pushButtonLogin->setText(tr("正在连接..."));
	SetConnectionStatus(tr("正在认证"), tr("正在连接吉林大学校园网服务器"), "#f79009");
	dogcomController->Login(account, password, mac_addr);
}

void MainWindow::on_buttonTogglePassword_clicked()
{
	const bool hidden = ui->lineEditPass->echoMode() == QLineEdit::Password;
	ui->lineEditPass->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
	passwordVisibilityAction->setIcon(QIcon(hidden ? ":/images/eye-off.svg" : ":/images/eye.svg"));
	passwordVisibilityAction->setToolTip(hidden ? tr("隐藏密码") : tr("显示密码"));
}

void MainWindow::SetConnectionStatus(const QString &title, const QString &detail, const QString &color)
{
	ui->labelStatus->setText(title);
	ui->labelStatusDetail->setText(detail);
	ui->statusDot->setStyleSheet(QString("QLabel { background-color: %1; border-radius: 6px; }").arg(color));
	auto *effect = qobject_cast<QGraphicsOpacityEffect *>(ui->statusPanel->graphicsEffect());
	if (!effect) {
		effect = new QGraphicsOpacityEffect(ui->statusPanel);
		ui->statusPanel->setGraphicsEffect(effect);
	}
	effect->setOpacity(0.72);
	auto *animation = new QPropertyAnimation(effect, "opacity", effect);
	animation->setDuration(220);
	animation->setStartValue(0.72);
	animation->setEndValue(1.0);
	animation->setEasingCurve(QEasingCurve::OutCubic);
	animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::ApplyTheme(Qt::ColorScheme scheme)
{
	const bool dark = scheme == Qt::ColorScheme::Dark;
	const QString style = dark ? QStringLiteral(R"(
QDialog { background: transparent; color: #f2f4f7; }
QLabel#titleLabel { font-size: 24px; font-weight: 700; color: #f5f8ff; }
QLabel#subtitleLabel { font-size: 13px; color: #aab4c3; }
QLabel#sectionLabel, QLabel#preferencesLabel { font-size: 14px; font-weight: 650; color: #e7ecf3; }
QLabel#connectionPreferencesLabel, QLabel#windowPreferencesLabel { font-size: 12px; font-weight: 600; color: #98a4b3; }
QLabel#fieldLabelAccount, QLabel#fieldLabelPassword, QLabel#fieldLabelMac { font-size: 12px; color: #aab4c2; }
QFrame#statusPanel { background: rgba(31, 38, 48, 188); border: 1px solid rgba(255, 255, 255, 32); border-radius: 9px; }
QLabel#statusDot { background: #98a2b3; border-radius: 6px; min-width: 12px; max-width: 12px; min-height: 12px; max-height: 12px; }
QLabel#labelStatus { font-size: 16px; font-weight: 650; color: #f5f7fa; }
QLabel#labelStatusDetail, QLabel#labelIp { color: #aab4c3; font-size: 12px; }
QLineEdit { min-height: 40px; border: 1px solid #46515f; border-radius: 8px; background: rgba(20, 25, 33, 195); color: #f6f8fb; padding: 0 12px; selection-background-color: #3b8bd9; font-size: 14px; }
QLineEdit:focus { border: 2px solid #5aa7ef; padding: 0 11px; }
QLineEdit:disabled { background: rgba(35, 42, 52, 180); color: #778391; }
QFrame#separator { background: rgba(255, 255, 255, 28); min-height: 1px; max-height: 1px; }
QCheckBox { spacing: 8px; color: #dce2ea; min-height: 24px; }
QCheckBox::indicator { width: 17px; height: 17px; }
QPushButton#pushButtonLogin { min-height: 46px; border: 0; border-radius: 8px; background: #3b8bd9; color: white; font-size: 15px; font-weight: 650; }
QPushButton#pushButtonLogin:hover { background: #4d9be5; }
QPushButton#pushButtonLogin:pressed { background: #2e78c2; }
QPushButton#pushButtonLogin:disabled { background: rgba(88, 111, 133, 150); color: #c4ccd5; }
QMenuBar { background: transparent; color: #f2f4f7; }
)") : QStringLiteral(R"(
QDialog { background: transparent; color: #182230; }
QLabel#titleLabel { font-size: 24px; font-weight: 700; color: #0b3566; }
QLabel#subtitleLabel { font-size: 13px; color: #667085; }
QLabel#sectionLabel, QLabel#preferencesLabel { font-size: 14px; font-weight: 650; color: #344054; }
QLabel#connectionPreferencesLabel, QLabel#windowPreferencesLabel { font-size: 12px; font-weight: 600; color: #667085; }
QLabel#fieldLabelAccount, QLabel#fieldLabelPassword, QLabel#fieldLabelMac { font-size: 12px; color: #475467; }
QFrame#statusPanel { background: rgba(255, 255, 255, 205); border: 1px solid rgba(190, 202, 216, 165); border-radius: 9px; }
QLabel#statusDot { background: #98a2b3; border-radius: 6px; min-width: 12px; max-width: 12px; min-height: 12px; max-height: 12px; }
QLabel#labelStatus { font-size: 16px; font-weight: 650; color: #1d2939; }
QLabel#labelStatusDetail, QLabel#labelIp { color: #667085; font-size: 12px; }
QLineEdit { min-height: 40px; border: 1px solid rgba(180, 194, 210, 195); border-radius: 8px; background: rgba(255, 255, 255, 205); color: #182230; padding: 0 12px; selection-background-color: #0b6fd3; font-size: 14px; }
QLineEdit:focus { border: 2px solid #287dcc; padding: 0 11px; }
QLineEdit:disabled { background: rgba(235, 239, 244, 185); color: #98a2b3; }
QFrame#separator { background: rgba(119, 134, 153, 42); min-height: 1px; max-height: 1px; }
QCheckBox { spacing: 8px; color: #344054; min-height: 24px; }
QCheckBox::indicator { width: 17px; height: 17px; }
QPushButton#pushButtonLogin { min-height: 46px; border: 0; border-radius: 8px; background: #0b63b6; color: white; font-size: 15px; font-weight: 650; }
QPushButton#pushButtonLogin:hover { background: #1673c6; }
QPushButton#pushButtonLogin:pressed { background: #084982; }
QPushButton#pushButtonLogin:disabled { background: rgba(139, 165, 188, 180); color: white; }
QMenuBar { background: transparent; }
)");
	setStyleSheet(style);
}

void MainWindow::SetDisableInput(bool yes)
{
	ui->lineEditAccount->setDisabled(yes);
	ui->lineEditPass->setDisabled(yes);
	ui->lineEditMAC->setDisabled(yes);
	ui->checkBoxRemember->setDisabled(yes);
	ui->checkBoxAutoLogin->setDisabled(yes);
	ui->checkBoxAutoReconnect->setDisabled(yes);
	ui->pushButtonLogin->setDisabled(yes);
}

void MainWindow::SetIcon(bool online)
{
	QIcon icon;
	QString toolTip;
	if (online) {
		icon = QIcon(":/images/tray-online.svg");
		toolTip = tr("JLU Link · 已连接");
	}
	else {
		icon = QIcon(":/images/tray-offline.svg");
		toolTip = tr("JLU Link · 未连接");
	}
	icon.setIsMask(true);
	trayIcon->setIcon(icon);
	trayIcon->setToolTip(toolTip);
	setWindowIcon(QIcon(":/images/AppIcon.png"));
}

void MainWindow::GetInputs() {
	account = ui->lineEditAccount->text();
	password = ui->lineEditPass->text();
	mac_addr = ui->lineEditMAC->text().trimmed().toUpper();
    bRemember = ui->checkBoxRemember->checkState();
    bAutoLogin = ui->checkBoxAutoLogin->checkState();
	bAutoReconnect = ui->checkBoxAutoReconnect->isChecked();
}

void MainWindow::HandleOffline(int reason)
{
	CURR_STATE = STATE_OFFLINE;
	ui->pushButtonLogin->setText(tr("连接校园网"));
	if (reason == OFF_USER_LOGOUT) {
		SetConnectionStatus(tr("未连接"), tr("已断开校园网连接"), "#98a2b3");
	} else if (reason == OFF_TIMEOUT) {
		SetConnectionStatus(tr("连接已中断"), tr("请检查网络后重新连接"), "#e5484d");
	} else {
		SetConnectionStatus(tr("认证未完成"), tr("请根据提示检查后重试"), "#e5484d");
	}
	switch (reason) {
	case OFF_USER_LOGOUT: {
        if(bQuit){
            qApp->quit();
            return;
        }
        if(bRestart){
            qApp->quit();
            qDebug() << "Restarting Drcom...";
            QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
            qDebug() << "Restart done.";
            return;
        }
		break;
	}
	case OFF_BIND_FAILED: {
		QMessageBox::critical(this, tr("Login failed"), tr("Binding port failed. Please check if there are other clients occupying the port"));
		break;
	}
	case OFF_CHALLENGE_FAILED: {
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Warning);
		msgBox.setWindowTitle(tr("无法连接认证服务器"));
		msgBox.setText(tr("请确认已连接校园 Wi-Fi 或插好网线，然后重新尝试。"));
		QAbstractButton *pButtonYes = msgBox.addButton(tr("重新启动并连接"), QMessageBox::AcceptRole);
		msgBox.addButton(tr("稍后处理"), QMessageBox::RejectRole);
		msgBox.exec();
		if (msgBox.clickedButton() == pButtonYes) {
			qDebug() << "Restart DrCOM confirmed in case OFF_CHALLENGE_FAILED";
			RestartDrcomByUser();
		}
		break;
	}
	case OFF_CHECK_MAC: {
		QMessageBox::critical(this, tr("Login failed"), tr("Someone is using this account with wired"));
		break;
	}
	case OFF_SERVER_BUSY: {
		QMessageBox::critical(this, tr("Login failed"), tr("The server is busy, please log back in again"));
		break;
	}
	case OFF_WRONG_PASS: {
		QMessageBox::critical(this, tr("Login failed"), tr("Account and password not match"));
		break;
	}
	case OFF_NOT_ENOUGH: {
		QMessageBox::critical(this, tr("Login failed"), tr("The cumulative time or traffic for this account has exceeded the limit"));
		break;
	}
	case OFF_FREEZE_UP: {
		QMessageBox::critical(this, tr("Login failed"), tr("This account is suspended"));
		break;
	}
	case OFF_NOT_ON_THIS_IP: {
		QMessageBox::critical(this, tr("Login failed"), tr("IP address does not match, this account can only be used in the specified IP address"));
		break;
	}
	case OFF_NOT_ON_THIS_MAC: {
		QMessageBox::critical(this, tr("Login failed"), tr("MAC address does not match, this account can only be used in the specified IP and MAC address"));
		break;
	}
	case OFF_TOO_MUCH_IP: {
		QMessageBox::critical(this, tr("Login failed"), tr("This account has too many IP addresses"));
		break;
	}
	case OFF_UPDATE_CLIENT: {
		QMessageBox::critical(this, tr("Login failed"), tr("The client version is incorrect"));
		break;
	}
	case OFF_NOT_ON_THIS_IP_MAC: {
		QMessageBox::critical(this, tr("Login failed"), tr("This account can only be used on specified MAC and IP address"));
		break;
	}
	case OFF_MUST_USE_DHCP: {
		QMessageBox::critical(this, tr("Login failed"), tr("Your PC set up a static IP, please change to DHCP, and then re-login"));
		break;
	}
	case OFF_TIMEOUT: {
		// 先尝试自己重启若干次，自个重启还不行的话再提示用户
		// 自己重启的话需要用户提前勾选记住密码
        if (bRemember && bAutoReconnect) {
			QSettings s(SETTINGS_FILE_NAME);
			int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
			qDebug() << "case OFF_TIMEOUT: restartTimes=" << restartTimes;
			if (restartTimes <= RETRY_TIMES) {
				qDebug() << "case OFF_TIMEOUT: restartTimes++";
				s.setValue(ID_RESTART_TIMES, restartTimes + 1);
				//尝试自行重启
				qDebug() << "trying to restart to reconnect...";
				qDebug() << "restarting for the" << restartTimes << "times";
				RestartDrcom();
				return;
			}
		}

		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Warning);
		msgBox.setWindowTitle(tr("校园网连接已中断"));
		msgBox.setText(tr("与认证服务器的连接已中断。请检查 Wi-Fi 或网线是否正常。"));
		QAbstractButton *pButtonYes = msgBox.addButton(tr("重新启动并连接"), QMessageBox::AcceptRole);
		msgBox.addButton(tr("稍后处理"), QMessageBox::RejectRole);
		msgBox.exec();
		if (msgBox.clickedButton() == pButtonYes) {
			qDebug() << "Restart DrCOM confirmed in case OFF_TIMEOUT";
			RestartDrcomByUser();
		}
		break;
	}
	case OFF_UNKNOWN:
	default:
		QMessageBox::warning(this, tr("连接已中断"), tr("校园网连接意外中断，请稍后重新连接。"));
		break;
	}
	if (reason == OFF_WRONG_PASS) {
		// 清除已保存的密码
		account = "";
		password = "";
        bRemember = false;
        bAutoLogin = false;
		bAutoReconnect = false;
		SaveSettings();
	}
	// 重新启用输入
	SetDisableInput(false);
	SetIcon(false);
	// 禁用注销按钮
	UpdateTrayMenu();
	// 显示出窗口
	ShowLoginWindow();
}

void MainWindow::HandleLoggedIn()
{
	QSettings s(SETTINGS_FILE_NAME);
	int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
	qDebug() << "HandleLoggedIn: restartTimes=" << restartTimes;

	CURR_STATE = STATE_ONLINE;
	SetConnectionStatus(tr("已连接"), tr("校园网认证成功，连接保持中"), "#18a96f");
	ui->pushButtonLogin->setText(tr("断开连接"));
	ui->pushButtonLogin->setEnabled(true);
	// 显示欢迎页
    bool bNotShowWelcome = s.value(ID_NOT_SHOW_WELCOME, false).toBool();
    if (restartTimes == 0 && !bNotShowWelcome) {
        qDebug()<<"open welcome page";
		QDesktopServices::openUrl(QUrl("http://login.jlu.edu.cn/notice.php"));
	}
	else {
		// 自行尝试重启的，不显示欢迎页
	}
	// 登录成功，保存密码
    if (bRemember) {
		SaveSettings();
	}
	else {
		account = "";
		password = "";
		SaveSettings();
	}
	SetIcon(true);
	// 启用注销按钮
	UpdateTrayMenu();

	s.setValue(ID_RESTART_TIMES, 0);
	qDebug() << "HandleLoggedIn: reset restartTimes";
}

void MainWindow::HandleIpAddress(const QString &ip)
{
	ui->labelIp->setText(tr("IP：%1").arg(ip));
}

void MainWindow::UserLogOut()
{
	// 用户主动注销
	dogcomController->LogOut();
	// 注销后应该是想重新登录或者换个号，因此显示出用户界面
	restoreAction->activate(restoreAction->Trigger);
	statusAction->setText(tr("状态：正在断开"));
	logOutAction->setEnabled(false);
}

void MainWindow::UpdateTrayMenu() {
	restoreAction->setText(isVisible() && !isMinimized() ? tr("隐藏主窗口") : tr("打开主窗口"));
	if (CURR_STATE == STATE_ONLINE) {
		statusAction->setText(tr("状态：已连接"));
		logOutAction->setText(tr("断开校园网"));
		logOutAction->setEnabled(true);
	} else if (CURR_STATE == STATE_LOGGING) {
		statusAction->setText(tr("状态：正在认证"));
		logOutAction->setText(tr("正在连接…"));
		logOutAction->setEnabled(false);
	} else {
		statusAction->setText(tr("状态：未连接"));
		logOutAction->setText(tr("连接校园网"));
		logOutAction->setEnabled(true);
	}
}

void MainWindow::on_checkBoxNotShowWelcome_toggled(bool checked)
{
    QSettings s(SETTINGS_FILE_NAME);
    s.setValue(ID_NOT_SHOW_WELCOME, checked);
}

void MainWindow::on_checkBoxHideLoginWindow_toggled(bool checked)
{
    QSettings s(SETTINGS_FILE_NAME);
    s.setValue(ID_HIDE_WINDOW, checked);
}
