#include "tabwidget.h"
#include <kybackup/backuptools-define.h>

TabWid::TabWid(QWidget *parent):QWidget(parent)
{
    allComponents();
}

void TabWid::initDbus()
{
    /* 更新管理器dbus接口 */
    updateMutual = UpdateDbus::getInstance();

    /* 源管理器dbus接口 */
    updateSource = new UpdateSource();

    QThread *tmp_thread = new QThread;
    connect(tmp_thread,&QThread::started,updateSource,&UpdateSource::startDbus);
    connect(updateSource,&UpdateSource::startDbusFinished,this,&TabWid::dbusFinished);
    updateSource->moveToThread(tmp_thread);
    tmp_thread->start();

    ukscConnect = new UKSCConn();

    /*备份还原初始化*/
    backup = new BackUp;
    backupThread = new QThread;
    backup->moveToThread(backupThread);
    backupThread->start();

    connect(updateMutual,&UpdateDbus::sendAppMessageSignal,this,&TabWid::loadingOneUpdateMsgSlot);
    connect(updateMutual,&UpdateDbus::sendFinishGetMsgSignal,this,&TabWid::loadingFinishedSlot);
    connect(checkUpdateBtn,&QPushButton::clicked,this,&TabWid::checkUpdateBtnClicked);
    connect(historyUpdateLog,&QPushButton::clicked,this,&TabWid::showHistoryWidget);
    connect(isAutoCheckSBtn,&SwitchButton::checkedChanged,this,&TabWid::isAutoCheckedChanged);
    connect(isAutoBackupSBtn,&SwitchButton::checkedChanged,this,&TabWid::isAutoBackupChanged);
    connect(isAutoUpgradeSBtn, &SwitchButton::checkedChanged, this, &TabWid::isAutoUpgradeChanged);
    connect(updateSource,&UpdateSource::getReplyFalseSignal,this,&TabWid::getReplyFalseSlot);
    //    bacupInit();//初始化备份
    isAutoBackupSBtn->setChecked(true);
    checkUpdateBtn->stop();
    //    checkUpdateBtn->setText(tr("检查更新"));

    checkUpdateBtn->setText(tr("Check Update"));

    checkUpdateBtn->setText(tr("initializing"));
    checkUpdateBtn->setEnabled(false);

}

void TabWid::isAutoUpgradeChanged()
{
    QStringList list;
    if (isAutoUpgradeSBtn->isChecked())
        list << "CONTROL_CENTER/autoupdate_allow" << "true";
    else
        list << "CONTROL_CENTER/autoupdate_allow" << "false";
    updateSource->getOrSetConf("set", list);
}

void TabWid::dbusFinished()
{
    /*获取自动更新开关的状态*/
    QStringList list;
    list << "CONTROL_CENTER/autoupdate_allow";
    QString ret =  updateSource->getOrSetConf("get", list);
    if (!ret.compare("false"))
        isAutoUpgradeSBtn->setChecked(false);
    else
        isAutoUpgradeSBtn->setChecked(true);

    /*获取当前自动更新的状态*/
    getAutoUpgradeStatus();
}

void TabWid::getAutoUpgradeStatus()
{
    fileLock();
    QStringList list;
    list << "CONTROL_CENTER/autoupdate_run_status";
    QString ret =  updateSource->getOrSetConf("get", list);

    if (!ret.compare("backup")) {
        isAutoUpgrade = true;
        /*如果自动更新在备份中，那就直接绑定备份还原信号即可*/
        isAllUpgrade = true;
        bool ret = autoUpdateLoadUpgradeList(true  );
        if (ret) {
            bacupInit(true);
            backup->creatInterface();
            backup->setProgress = true;
            backupProgress(0);
        } else {
            bacupInit(false);
        }
    } else if (!ret.compare("download")) {
        /*如果自动更新在下载中，调用dbus去kill掉下载程序，继续原流程，不进行多余操作*/
        QFile file("/var/run/apt-download.pid");
        QString pid;
        if (file.open(QIODevice::ReadOnly)) {
            pid = file.readAll();
        }
        file.close();
        updateSource->killProcessSignal(pid.toInt(), 10);
        checkUpdateBtn->setEnabled(true);
        checkUpdateBtn->setText(tr("Check Update"));
        checkUpdateBtnClicked();
    } else if (!ret.compare("install")){
        isAutoUpgrade = true;
        /*如果自动更新在安装中，绑定更新管理器dbus接收信号即可*/
        isAllUpgrade = true;
        checkUpdateBtn->hide();
        checkUpdateBtn->setText(tr("UpdateAll"));
        autoUpdateLoadUpgradeList(false);
    } else if (!ret.compare("idle")) {
        /*如果没有进行自动更新，那就不需要操作 */
        checkUpdateBtn->setEnabled(true);
        checkUpdateBtn->setText(tr("Check Update"));
        checkUpdateBtnClicked();
    } else {
        /*如果读不到，默认也不进行操作*/
        checkUpdateBtn->setEnabled(true);
        checkUpdateBtn->setText(tr("Check Update"));
        checkUpdateBtnClicked();
    }
}

bool TabWid::autoUpdateLoadUpgradeList(bool isBackUp)
{
    QSettings get("/var/lib/kylin-auto-upgrade/kylin-autoupgrade-pkglist.conf", QSettings::IniFormat);
    QString str;
    if (isBackUp)
        str = get.value("DOWNLOAD/pkgname").toString();
    else
        str = get.value("DOWNLOAD/uninstpkg").toString();
    qDebug() << "----------pkgname---->" << str;

    if (str.isNull()) {
        versionInformationLab->setText(tr("Your system is the latest!"));
        QString updatetime = tr("No Information!");
        QSqlQuery queryInstall(QSqlDatabase::database("A"));
        queryInstall.exec("select * from installed order by id desc");
        while (queryInstall.next()) {
            QString statusType = queryInstall.value("keyword").toString();
            if (statusType == "" || statusType =="1") {
                updatetime = queryInstall.value("time").toString();
                break;
            }
        }
        lastRefreshTime->setText(tr("Last refresh:")+ updatetime);
        lastRefreshTime->show();
        checkUpdateBtn->setText(tr("Check Update"));
        return false;
    } else {
        QStringList list;
        if(str.contains(" ")) {
            list = str.split(" ");
        } else {
            list << str;
        }
        versionInformationLab->setText(tr("Downloading and installing updates..."));
        lastRefreshTime->hide();
        allProgressBar->show();
        allProgressBar->setValue(10);
        updateMutual->getAppMessage(list);
        return true;
    }
}

void TabWid::unableToConnectSource()
{
    qDebug() << "源管理器信号是否连接成功：" << isConnectSourceSignal;
    if(isConnectSourceSignal == false)
    {
        disconnectSource();
    }
}
void TabWid::disconnectSource()
{
    disconnect(updateSource->serviceInterface,SIGNAL(updateTemplateStatus(QString)),this,SLOT(slotUpdateTemplate(QString)));
    disconnect(updateSource->serviceInterface,SIGNAL(updateCacheStatus(QVariantList)),this,SLOT(slotUpdateCache(QVariantList)));
    disconnect(updateSource->serviceInterface,SIGNAL(updateSourceProgress(QVariantList)),this,SLOT(slotUpdateCacheProgress(QVariantList)));
    disconnect(updateSource , &UpdateSource::sigReconnTimes , this , &TabWid::slotReconnTimes);
    checkUpdateBtn->setEnabled(true);
    checkUpdateBtn->stop();
    //        checkUpdateBtn->setText(tr("检查更新"));
    checkUpdateBtn->setText(tr("Check Update"));
    //        versionInformationLab->setText(tr("服务连接异常，请重新检测!") );
    versionInformationLab->setText(tr("Failed to connect to software warehouse!") );
}
TabWid::~TabWid()
{
    qDebug() << "~TabWid" ;
    delete updateMutual;
    updateMutual = nullptr;
    backupDelete();//回收资源

    fileUnLock();
}

void TabWid::backupMessageBox(QString str)
{
    QMessageBox msgBox;
    msgBox.setText(str);
    msgBox.addButton(tr("Update now"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel update"), QMessageBox::RejectRole);
    int ret = msgBox.exec();
    if(ret == 0)
    {
        qDebug() << "立即更新!";
        checkUpdateBtn->start();
        checkUpdateBtn->setEnabled(false);
        versionInformationLab->setText(tr("Being updated..."));
        //         versionInformationLab->setText("正在更新...");
        updateMutual->isPointOutNotBackup = false;   //全部更新时不再弹出单个更新未备份提示
        emit updateAllSignal();
    }
    else if(ret == 1)
    {
        bacupInit(false);
        qDebug() << "不进行全部更新。";
        checkUpdateBtn->stop();
        checkUpdateBtn->setEnabled(true);
        //       checkUpdateBtn->setText(tr("全部更新"));
        versionInformationLab->setText(tr("Updatable app detected on your system!"));
        checkUpdateBtn->setText(tr("UpdateAll"));
        foreach (AppUpdateWid *wid, widgetList) {
            wid->updateAPPBtn->show();
        }
    }
}

void TabWid::backupCore()
{
    int initresult = emit needBackUp();
    switch (initresult) {
    case -1:
        backupMessageBox(tr("The backup restore partition could not be found. The system will not be backed up in this update!"));
        //如果是则立即更新,否的话取消全部更新
        return;
    case -2:
        versionInformationLab->setText(tr("Kylin backup restore tool is doing other operations, please update later."));
        return;
    case -3:
        versionInformationLab->setText(tr("The source manager configuration file is abnormal, the system temporarily unable to update!"));
        return;
    case -4:
        versionInformationLab->setText(tr("Backup already, no need to backup again."));
        checkUpdateBtn->start();
        checkUpdateBtn->setEnabled(false);
        updateMutual->isPointOutNotBackup = false;   //全部更新时不再弹出单个更新未备份提示
        emit updateAllSignal();
        return;
    case 1://正在备份
        emit startBackUp(0);
        versionInformationLab->setText(tr("Start backup,getting progress")+"...");
        break;
    case needBack://需要备份
        emit startBackUp(1);
        break;
    case -9://备份还原工具不存在
        backupMessageBox(tr("Kylin backup restore tool does not exist, this update will not backup the system!"));
        return;
    default:
        qDebug()<<"备份还原工具状态码"<<initresult;
        return;
    }
    qDebug()<<"符合备份工具运行条件";
}

void TabWid::getAllProgress (QString pkgName, int Progress, QString type)
{
    if (!isAllUpgrade)
        return ;
    qDebug() << pkgName << Progress << type;
    versionInformationLab->setText(tr("Downloading and installing updates..."));
    checkUpdateBtn->setText(tr("Cancel"));
    checkUpdateBtn->setEnabled(false);
    for(int i = 0; i < pkgList.size(); i++) {
        if (!pkgList.at(i).name.compare(pkgName)) {
            if (!type.compare("download")) {
                pkgList.at(i).downloadProgress = Progress;
            }
            else {
                pkgList.at(i).installProgress = Progress;
            }
        }
    }
    int all = 0;
    foreach (pkgProgress msg, pkgList) {
        all += msg.downloadProgress;
        all += msg.installProgress;
    }

    int allNum = pkgList.size() * 200 ;
    float progress = (float)all / allNum;

    if (progress < 1)
        versionInformationLab->setText(tr("Downloading and installing updates..."));

    allProgressBar->setValue(progress * 100);
    allProgressBar->show();
    lastRefreshTime->hide();
    if (progress == 1) {
        foreach (AppUpdateWid *wid, widgetList) {
            disconnect(wid, &AppUpdateWid::sendProgress, this, &TabWid::getAllProgress);
        }
        checkUpdateBtn->hide();
        allProgressBar->hide();
        versionInformationLab->setText(tr("Your system is the latest!"));
    }
}
void TabWid::backupProgress(int progress)
{
    qDebug() <<"备份进度：---->" << progress;
    if(progress==100)
    {
        bacupInit(false);
        //备份完成，开始安装
        qDebug()<<"备份完成，开始安装";
        versionInformationLab->setText(tr("Backup complete."));
        return;
    }
    versionInformationLab->setText(tr("System is backing up..."));
    allProgressBar->setValue(progress);
    checkUpdateBtn->hide();
    allProgressBar->show();
    lastRefreshTime->hide();
}

void TabWid::bakeupFinish(int result)
{
    switch (result) {
    case -20:
        //        versionInformationLab->setText(tr("备份过程被中断，停止更新！"));
        versionInformationLab->setText(tr("Backup interrupted, stop updating!"));
        break;
    case 99:
        //        versionInformationLab->setText(tr("备份完成！"));
        versionInformationLab->setText(tr("Backup finished!"));
        break;
    default:
        //        backupMessageBox(tr("麒麟备份还原工具异常：")+QString::number(result)+","+tr("本次更新不会备份系统！"));
        backupMessageBox(tr("Kylin backup restore tool exception:")+QString::number(result)+","+tr("There will be no backup in this update!"));
        break;
    }
}

void TabWid::backupHideUpdateBtn(int result)
{
    if(result == 99)
    {
        checkUpdateBtn->start();
        //        versionInformationLab->setText("正在更新...");
        versionInformationLab->setText(tr("Being updated..."));
        updateMutual->isPointOutNotBackup = false;   //全部更新时不再弹出单个更新未备份提示
        emit updateAllSignal();
    }
    else if(result == -20)
    {
        checkUpdateBtn->stop();
        checkUpdateBtn->setEnabled(true);
        //        checkUpdateBtn->setText(tr("全部更新"));
        checkUpdateBtn->setText(tr("UpdateAll"));

    }
}

void TabWid::bacupInit(bool isConnect)
{
    qDebug() << "======>tabwid info: " <<backupThread ;
    connect(this,&TabWid::needBackUp,backup,&BackUp::needBacdUp,Qt::BlockingQueuedConnection);//同步信号，阻塞，取返回值
    if (isConnect) {
        qDebug() << "is connect ;;";
        connect(this,&TabWid::startBackUp,backup,&BackUp::startBackUp);
        connect(backup, &BackUp::calCapacity, this, &TabWid::whenStateIsDuing);
        connect(backup, &BackUp::backupStartRestult, this, &TabWid::receiveBackupStartResult);
        connect(backup,&BackUp::bakeupFinish,this,&TabWid::bakeupFinish);
        connect(backup,&BackUp::backupProgress,this,&TabWid::backupProgress);
        connect(backup,&BackUp::bakeupFinish,this,&TabWid::backupHideUpdateBtn);
    } else {
        qDebug() << "is disconnect;;";
        disconnect(this,&TabWid::startBackUp,backup,&BackUp::startBackUp);
        disconnect(backup, &BackUp::calCapacity, this, &TabWid::whenStateIsDuing);
        disconnect(backup, &BackUp::backupStartRestult, this, &TabWid::receiveBackupStartResult);
        disconnect(backup,&BackUp::bakeupFinish,this,&TabWid::bakeupFinish);
        disconnect(backup,&BackUp::backupProgress,this,&TabWid::backupProgress);
        disconnect(backup,&BackUp::bakeupFinish,this,&TabWid::backupHideUpdateBtn);
    }

}

void TabWid::backupDelete()
{
    backup->deleteLater();
    backupThread->deleteLater();
}

void TabWid::slotUpdateTemplate(QString status)
{
    qDebug() <<  "源管理器：" <<"update template status :" << status;

}
void TabWid::slotUpdateCache(QVariantList sta)
{

    QString status = sta.at(1).toString();
    QString nowsymbol = sta.at(0).toString();
    qDebug() << "源管理器：" <<"slotUpdateCache" << "nowsymbol" <<nowsymbol << status;
    if(nowsymbol == Symbol)
    {
        isConnectSourceSignal = true;
        qDebug() <<"源管理器：" << "update cache status :" << status;
        if (!status.compare("success")) {
//            int progress = 100;
//            versionInformationLab->setText(tr("Update software source :") + QString::number(progress)+"%");
            versionInformationLab->setText(tr("Getting update list")+"...");

            QFile file(IMPORTANT_FIEL_PATH);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qDebug() <<"file open failed!" << IMPORTANT_FIEL_PATH;
                versionInformationLab->setText(tr("Software source update failed: "));  //软件源更新失败
                checkUpdateBtn->setEnabled(true);
                checkUpdateBtn->stop();
                checkUpdateBtn->setText(tr("Check Update"));
                return ;
            }
            QString str =  file.readAll();
            QStringList list;
            str = str.simplified();
            if (!str.isEmpty()) {
                list = str.split(" ");
            }
            qDebug() << "slotUpdateCache函数：获取到的包列表：" << list;
//            versionInformationLab->setText(tr("Getting update list")+"...");
            updateMutual->getAppMessage(list);

        }
        else
        {
            int statuscode = status.toInt();
            if(statuscode == 100)
            {
                //updateSource->callDBusUpdateTemplate();
                qDebug() << "源管理器：" <<"statuscode = :" << statuscode;
                QString failedInfo = updateSource->getFailInfo(statuscode);
                qDebug() << "源管理器：" <<"failedInfo:" << failedInfo;
            }
            else
            {
                QString failedInfo = updateSource->getFailInfo(statuscode);
                checkUpdateBtn->setEnabled(true);
                checkUpdateBtn->stop();
                //                checkUpdateBtn->setText(tr("检查更新"));
                checkUpdateBtn->setText(tr("Check Update"));
                //                versionInformationLab->setText(tr("软件源更新失败：")+failedInfo );
                versionInformationLab->setText(tr("Software source update failed: ")+failedInfo );
                disconnect(updateSource->serviceInterface,SIGNAL(updateTemplateStatus(QString)),this,SLOT(slotUpdateTemplate(QString)));
                disconnect(updateSource->serviceInterface,SIGNAL(updateCacheStatus(QVariantList)),this,SLOT(slotUpdateCache(QVariantList)));
                disconnect(updateSource->serviceInterface,SIGNAL(updateSourceProgress(QVariantList)),this,SLOT(slotUpdateCacheProgress(QVariantList)));
            }
        }
    }
}
void TabWid::slotUpdateCacheProgress(QVariantList pro)
{
    isConnectSourceSignal = true;
    int progress = pro.at(1).toInt();
    if (progress > 100) {
        progress = 100;
    }
    QString nowsymbol = pro.at(0).toString();
        qDebug() << "update cache progress :" << progress;
    if(nowsymbol == Symbol)
    {
            versionInformationLab->setText(tr("Update software source :") + QString::number(progress)+"%");
            //            versionInformationLab->setText(tr("更新软件源进度：") + QString::number(progress)+"%");
    }
}

void TabWid::allComponents()
{
    mainTabLayout = new QVBoxLayout();  //整个页面的主布局
    scrollArea = new QScrollArea(this);
    updateTab = new QWidget(this);  //更新页面

    AppMessage = new QVBoxLayout();
    AppMessage->setAlignment(Qt::AlignTop);
    updateTab->setLayout(AppMessage);
    systemWidget = new QFrame(updateTab);
    systemWidget->setFrameShape(QFrame::Box);
    systemPortraitLab = new QLabel(systemWidget);

    allUpdateWid = new QWidget(this);
    allUpdateLayout = new QVBoxLayout();
    allUpdateLayout->setAlignment(Qt::AlignTop);
    allUpdateWid->setLayout(allUpdateLayout);
    allUpdateLayout->setSpacing(2);
    allUpdateLayout->setMargin(0);

    labUpdate = new TitleLabel(this);
    //    labUpdate->setText(tr("更新"));
    labUpdate->setText(tr("Update"));
    labUpdate->adjustSize();
    labUpdate->setFixedHeight(25);

    scrollArea->setWidget(updateTab);
    scrollArea->setFrameStyle(0);
    //    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scrollArea->setWidgetResizable(true);

    systemWidget->resize(560,140);
    systemWidget->setFixedHeight(140);

    systemPortraitLab = new QLabel();
    tab1HLayout = new QHBoxLayout();
    systemWidget->setLayout(tab1HLayout);
    systemPortraitLab->setFixedSize(96,96);
    systemPortraitLab->setPixmap(QPixmap(":/img/plugins/upgrade/normal.png").scaled(QSize(96,96)));

    QWidget *historyUpdateLogWid = new QWidget(this);
    QHBoxLayout *historyUpdateLogLayout = new QHBoxLayout();
    historyUpdateLog = new QPushButton(this); // 历史日志弹出窗口控制按钮
    //    historyUpdateLog->setText(tr("查看更新历史"));
    historyUpdateLog->setText(tr("View history"));
    historyUpdateLogWid->setLayout(historyUpdateLogLayout);
    historyUpdateLogLayout->setAlignment(Qt::AlignLeft);
    historyUpdateLogLayout->addWidget(historyUpdateLog);
    historyUpdateLogLayout->setSpacing(0);
    historyUpdateLogLayout->setMargin(0);


    inforLayout = new QVBoxLayout();
    lastRefreshTime = new QLabel(systemWidget);
    allProgressBar = new QProgressBar(systemWidget);
    allProgressBar->setFixedHeight(4);
    allProgressBar->hide();
    allProgressBar->setValue(0);

    progressLabel = new QLabel(systemWidget);
    //    progressLabel->setText(tr("45M/300M"));

    versionInformationLab = new QLabel(systemWidget);
    versionInformationLab->setStyleSheet("font-size:16px;font-weight:500;line-height: 22px;");

    inforLayout->setAlignment(Qt::AlignTop);
    inforLayout->addWidget(versionInformationLab);
    inforLayout->addWidget(lastRefreshTime);
    inforLayout->addWidget(allProgressBar);
    inforLayout->addWidget(progressLabel);
    inforLayout->setSpacing(8);
    inforLayout->setContentsMargins(8,20,0,0);
    QWidget *inforWidget = new QWidget(systemWidget);
    inforWidget->setLayout(inforLayout);

    checkUpdateBtn = new m_button(systemWidget);
    checkUpdateBtn->setDefault(true);
    checkUpdateBtn->start();
    checkUpdateBtn->setFixedSize(120,36);

    tab1HLayout->addWidget(systemPortraitLab,0,Qt::AlignLeft);
    tab1HLayout->addWidget(inforWidget,Qt::AlignLeft|Qt::AlignTop);
    tab1HLayout->addSpacerItem(new QSpacerItem(50, 2, QSizePolicy::Fixed));
    tab1HLayout->addWidget(checkUpdateBtn);
    tab1HLayout->setContentsMargins(8,25,20,20);
    tab1HLayout->setSpacing(0);

    updateSettingLab = new TitleLabel();
    //    updateSettingLab->setText(tr("更新设置"));
    //~ contents_path /upgrade/Update Settings
    updateSettingLab->setText(tr("Update Settings"));
    updateSettingLab->setFixedHeight(25);
    updateSettingWidget = new QWidget(this);
    updatesettingLayout = new QVBoxLayout();
    updateSettingWidget->setLayout(updatesettingLayout);

    isAutoCheckWidget = new QFrame();
    isAutoCheckWidget->setFrameShape(QFrame::Box);
    isAutoCheckLayout = new QHBoxLayout();
    isAutoCheckedLab = new QLabel();
    //    isAutoCheckedLab->setText(tr("允许通知可更新的应用"));
    isAutoCheckedLab->setText(tr("Allowed to renewable notice"));
    isAutoCheckSBtn = new SwitchButton();
    isAutoCheckWidget->setLayout(isAutoCheckLayout);
    isAutoCheckLayout->addWidget(isAutoCheckedLab);
    isAutoCheckLayout->addWidget(isAutoCheckSBtn);

    isAutoBackupWidget = new QFrame();
    isAutoBackupWidget->setFrameShape(QFrame::Box);
    isAutoBackupLayout = new QHBoxLayout();
    isAutoBackupLab = new QLabel();
    isAutoBackupLab->setText(tr("Backup current system before updates all"));
    //    isAutoBackupLab->setText(tr("全部更新前备份系统"));
    isAutoBackupSBtn = new SwitchButton();

    isAutoBackupLayout->addWidget(isAutoBackupLab);
    isAutoBackupLayout->addWidget(isAutoBackupSBtn);
    isAutoBackupWidget->setLayout(isAutoBackupLayout);
    /*是否自动更新选项*/
    isAutoUpgradeWidget = new QFrame();
    isAutoUpgradeWidget->setFrameShape(QFrame::Box);
    isAutoUpgradeLayout = new QVBoxLayout();
    isAutoUpgradeLab = new QLabel();
    isAutoUpgradeLab->setText(tr("Automatically download and install updates"));
    autoUpgradeLab = new QLabel();
    autoUpgradeLab->setText(tr("After it is turned on, the system will automatically download and install updates when there is an available network and available backup and restore partitions."));
    /*自动换行*/
    autoUpgradeLab->adjustSize();
    autoUpgradeLab->setWordWrap(true);
    autoUpgradeLab->setAlignment(Qt::AlignTop);
    QPalette labelPalette = autoUpgradeLab->palette();
    labelPalette.setBrush(QPalette::WindowText, labelPalette.color(QPalette::PlaceholderText));
    autoUpgradeLab->setPalette(labelPalette);

    isAutoUpgradeSBtn = new SwitchButton();
    autoUpgradeBtnLayout = new QHBoxLayout();
    autoUpgradeBtnLayout->addWidget(isAutoUpgradeLab);
    autoUpgradeBtnLayout->addWidget(isAutoUpgradeSBtn);

    isAutoUpgradeLayout->addLayout(autoUpgradeBtnLayout);
    isAutoUpgradeLayout->addWidget(autoUpgradeLab);
    isAutoUpgradeWidget->setLayout(isAutoUpgradeLayout);

    updatesettingLayout->setAlignment(Qt::AlignTop);
    updatesettingLayout->addWidget(updateSettingLab);
    updatesettingLayout->addSpacing(10);
    updatesettingLayout->addWidget(isAutoCheckWidget);
    //    updatesettingLayout->addWidget(isAutoBackupWidget);
    updatesettingLayout->addWidget(isAutoUpgradeWidget);
    updatesettingLayout->setSpacing(2);
    updatesettingLayout->setMargin(0);

    AppMessage->addWidget(labUpdate);
    AppMessage->addWidget(systemWidget);
    AppMessage->addWidget(allUpdateWid);
    AppMessage->addWidget(historyUpdateLogWid);
    AppMessage->addSpacing(30);
    AppMessage->addWidget(updateSettingWidget);
    AppMessage->addStretch();
    AppMessage->setContentsMargins(0,0,28,10);

    mainTabLayout->setAlignment(Qt::AlignTop);
    mainTabLayout->addWidget(scrollArea);
    //    mainTabLayout->setSpacing(0);
    mainTabLayout->setMargin(0);
    this->setLayout(mainTabLayout);
    getAllDisplayInformation();

}

void TabWid::loadingOneUpdateMsgSlot(AppAllMsg msg)
{
    //    checkUpdateBtn->setText();
    if(updateMutual->importantList.indexOf(msg.name) == -1)
    {
        struct pkgProgress pkg;
        pkg.name = msg.name;
        pkgList << pkg;
        updateMutual->importantList.append(msg.name);   //重要更新列表中添加appname
        AppUpdateWid *appWidget = new AppUpdateWid(msg, this);
        widgetList << appWidget;
        connect(appWidget, &AppUpdateWid::cancel, this, &TabWid::slotCancelDownload);
        connect(this, &TabWid::updateAllSignal, appWidget, &AppUpdateWid::updateAllApp);
        connect(appWidget,&AppUpdateWid::hideUpdateBtnSignal,this,&TabWid::hideUpdateBtnSlot);
        connect(appWidget,&AppUpdateWid::changeUpdateAllSignal,this,&TabWid::changeUpdateAllSlot);
        connect(updateMutual,&UpdateDbus::sendFinishGetMsgSignal,appWidget,&AppUpdateWid::showUpdateBtn);
        connect(appWidget,&AppUpdateWid::filelockedSignal,this,&TabWid::waitCrucialInstalled);
        connect(backup,&BackUp::bakeupFinish,appWidget,&AppUpdateWid::hideOrShowUpdateBtnSlot);
        /*判断是否是后台自动更新*/
        if (isAutoUpgrade) {
            connect(appWidget, &AppUpdateWid::sendProgress, this, &TabWid::getAllProgress);
            appWidget->isUpdateAll = true;
            appWidget->isAutoUpgrade = true;
            appWidget->updateAPPBtn->hide();
            appWidget->appVersion->setText(tr("Ready to install"));
        }
        if(ukscConnect->isConnectUskc == true)
        {
            QStringList list = ukscConnect->getInfoByName(msg.name);
            if(list[2] != "")
            {
                appWidget->appNameLab->setText(list[2]);
                appWidget->dispalyName = list[2];

            }
            if(list[1] != "" && QLocale::system().name()=="zh_CN")
            {
                appWidget->appNameLab->setText(list[1]);
                appWidget->dispalyName = list[1];
            }
            if(list[0] != "" && !appWidget->haveThemeIcon)
            {
                appWidget->appIcon->setPixmap(QPixmap(list[0]).scaled(32, 32));
            }
        }
        allUpdateLayout->addWidget(appWidget);
        qDebug() << "更新管理器：" << "loadingOneUpdateMsgSlot:" << appWidget->dispalyName;
    }

}

void TabWid::loadingFinishedSlot(int size)
{
    if (isAutoUpgrade)
        bacupInit(true);
    else
        bacupInit(false);
    disconnect(updateSource->serviceInterface,SIGNAL(updateTemplateStatus(QString)),this,SLOT(slotUpdateTemplate(QString)));
    disconnect(updateSource->serviceInterface,SIGNAL(updateCacheStatus(QVariantList)),this,SLOT(slotUpdateCache(QVariantList)));
    disconnect(updateSource->serviceInterface,SIGNAL(updateSourceProgress(QVariantList)),this,SLOT(slotUpdateCacheProgress(QVariantList)));
    qDebug()<< "更新管理器：" <<"加载完毕信号 " << "size = " << size;
    if(updateMutual->importantList.size() == 0) {
        checkUpdateBtn->setEnabled(true);
        checkUpdateBtn->stop();
        //        checkUpdateBtn->setText(tr("检查更新"));
        checkUpdateBtn->setText(tr("Check Update"));
        //        versionInformationLab->setText(tr("您的系统已是最新！"));
        versionInformationLab->setText(tr("Your system is the latest!"));
        foreach (AppUpdateWid *wid, widgetList) {
            disconnect(wid, &AppUpdateWid::sendProgress, this, &TabWid::getAllProgress);
        }
        allProgressBar->hide();
        QString updatetime = tr("No Information!");
        QSqlQuery queryInstall(QSqlDatabase::database("A"));
        queryInstall.exec("select * from installed order by id desc");
        while (queryInstall.next()) {
            QString statusType = queryInstall.value("keyword").toString();
            if (statusType == "" || statusType =="1") {
                updatetime = queryInstall.value("time").toString();
                break;
            }
        }
        lastRefreshTime->setText(tr("Last refresh:")+ updatetime);
        lastRefreshTime->show();
        allProgressBar->hide();
    }
    else {
        updateMutual->importantSize = updateMutual->importantList.size();   //此次检测结果的更新数量
        checkUpdateBtn->stop();
        checkUpdateBtn->setEnabled(true);
        //        checkUpdateBtn->setText(tr("全部更新"));
        checkUpdateBtn->setText(tr("UpdateAll"));
        //        versionInformationLab->setText(tr("检测到你的系统有可更新的应用！"));
        if (!isAutoUpgrade) {
            versionInformationLab->setText(tr("Updatable app detected on your system!"));
        }

        systemPortraitLab->setPixmap(QPixmap(":/img/plugins/upgrade/update.png").scaled(96,96));
    }

}

void TabWid::getAllDisplayInformation()
{
    QSqlQuery query(QSqlDatabase::database("A"));
    QString updatetime;
    QString checkedtime;
    QString checkedstatues;
    QString backupStatus;
    query.exec("select * from display");
    while(query.next())
    {
        checkedtime = query.value("check_time").toString();
        checkedstatues = query.value("auto_check").toString();
        backupStatus = query.value("auto_backup").toString();
    }
    QSqlQuery queryInstall(QSqlDatabase::database("A"));
    updatetime = tr("No Information!");
    queryInstall.exec("select * from installed order by id desc");
    while(queryInstall.next())
    {
        QString statusType = queryInstall.value("keyword").toString();
        if(statusType == "" || statusType =="1") {
            updatetime = queryInstall.value("time").toString();
            break;
        }
    }
    lastRefreshTime->setText(tr("Last refresh:") + updatetime);
    versionInformationLab->setText(tr("Last Checked:")+checkedtime);
    if(checkedstatues == "false")
    {
        isAutoCheckSBtn->setChecked(false);
    }
    else
    {
        isAutoCheckSBtn->setChecked(true);
    }
    isAutoCheckSBtn->setEnabled(true);

    if(backupStatus == "false")
    {
        isAutoBackupSBtn->setChecked(false);
    }
    else
    {
        isAutoBackupSBtn->setChecked(true);
    }
}
void TabWid::showHistoryWidget()
{

    historyLog = m_updatelog::GetInstance(this);
    //在屏幕中央显示
    //QRect availableGeometry = qApp->primaryScreen()->availableGeometry();
    //historyLog->move((availableGeometry.width()-historyLog->width())/2,(availableGeometry.height()- historyLog->height())/2);
    historyLog->show();
}

void TabWid::slotReconnTimes(int times) {
    qDebug() << "更新模板失败 ， 重新连接 " << times << "次";
    versionInformationLab->setText(tr("trying to reconnect ") + QString::number(times) + tr(" times"));
}

void TabWid::checkUpdateBtnClicked()
{
    if(checkUpdateBtn->text() == tr("Check Update"))
    {
        widgetList.clear();
        connect(updateSource->serviceInterface,SIGNAL(updateTemplateStatus(QString)),this,SLOT(slotUpdateTemplate(QString)));
        connect(updateSource->serviceInterface,SIGNAL(updateCacheStatus(QVariantList)),this,SLOT(slotUpdateCache(QVariantList)));
        connect(updateSource->serviceInterface,SIGNAL(updateSourceProgress(QVariantList)),this,SLOT(slotUpdateCacheProgress(QVariantList)));
        connect(updateSource , &UpdateSource::sigReconnTimes , this , &TabWid::slotReconnTimes);
        updateMutual->failedList.clear();
        QList<AppUpdateWid*> list = this->findChildren<AppUpdateWid*>();
        for(AppUpdateWid* tmp:list)
        {
            tmp->deleteLater();
        }
        updateSource->callDBusUpdateTemplate();
        //        versionInformationLab->setText(tr("正在更新软件源..."));
        versionInformationLab->setText(tr("Updating the software source")+"...");
        //判断信号连接超时
        QTimer *timer = new QTimer;
        timer->setSingleShot(true);
        connect(timer,&QTimer::timeout,this,&TabWid::unableToConnectSource);
        timer->start(60000);
        QDateTime current_date_time = QDateTime::currentDateTime();
        QString current_date = current_date_time.toString("yyyy.MM.dd hh:mm:ss");
        updateMutual->insertInstallStates("check_time",current_date);
        checkUpdateBtn->start();
        checkUpdateBtn->setEnabled(false);
    }
    else if(checkUpdateBtn->text() == tr("UpdateAll"))
    {
        if (!get_battery()) {
            QMessageBox msgBox;
            msgBox.setText(tr("The battery is below 50% and the update cannot be downloaded"));
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setButtonText(QMessageBox::Ok,tr("OK"));
            msgBox.exec();
            return ;
        }


        QMessageBox msgBox(this);
        msgBox.setText(tr("Please back up the system before all updates to avoid unnecessary losses"));
        msgBox.setWindowTitle(tr("Prompt information"));

        msgBox.addButton(tr("Only Update"), QMessageBox::YesRole);
        msgBox.addButton(tr("Back And Update"), QMessageBox::AcceptRole);
        msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);

        foreach (AppUpdateWid *wid, widgetList) {
            connect(wid, &AppUpdateWid::sendProgress, this, &TabWid::getAllProgress);
            wid->updateAPPBtn->hide();
        }
        isAllUpgrade = true;
        int ret = msgBox.exec();
        switch (ret) {
        case 0:
            qDebug() << "全部更新。。。。。。";
            isAutoBackupSBtn->setChecked(false);
            checkUpdateBtn->setEnabled(false);
            checkUpdateBtn->start();
            updateMutual->isPointOutNotBackup = false;   //全部更新时不再弹出单个更新未备份提示
            emit updateAllSignal();
            break;
        case 1:
            bacupInit(true);
            backupCore();
            qDebug() << "否，立即备份";
            break;
        case 2:
            foreach (AppUpdateWid *wid, widgetList) {
                disconnect(wid, &AppUpdateWid::sendProgress, this, &TabWid::getAllProgress);
                wid->updateAPPBtn->show();
            }
            isAllUpgrade = false;
            qDebug() << "Close 暂不更新!";
            break;
        default:
            foreach (AppUpdateWid *wid, widgetList) {
                disconnect(wid, &AppUpdateWid::sendProgress, this, &TabWid::getAllProgress);
                wid->updateAPPBtn->show();
            }
            isAllUpgrade = false;
            qDebug() << "Close 暂不更新!";
            break;
        }
    }
}

void TabWid::isAutoCheckedChanged()     //自动检测按钮绑定的槽函数
{
    if(isAutoCheckSBtn->isChecked() == false)
    {
        updateMutual->insertInstallStates("auto_check","false");

    }
    else if(isAutoCheckSBtn->isChecked() == true)
    {
        updateMutual->insertInstallStates("auto_check","true");
    }
}

void TabWid::isAutoBackupChanged()
{
    if(isAutoBackupSBtn->isChecked() == false)
    {
        updateMutual->insertInstallStates("auto_backup","false");

    }
    else if(isAutoBackupSBtn->isChecked() == true)
    {
        updateMutual->insertInstallStates("auto_backup","true");
    }
}

void TabWid::slotCancelDownload()
{
    checkUpdateBtn->stop();
    //    checkUpdateBtn->setText("全部更新");
    checkUpdateBtn->setText(tr("UpdateAll"));
    checkUpdateBtn->setCheckable(true);
}

void TabWid::hideUpdateBtnSlot(bool isSucceed)
{
    Q_UNUSED(isSucceed);
    if(updateMutual->importantList.size() == 0) {
        checkUpdateBtn->setEnabled(true);
        checkUpdateBtn->stop();
        //        checkUpdateBtn->setText(tr("检查更新"));
        checkUpdateBtn->setText(tr("Check Update"));
        if(updateMutual->failedList.size() == 0) {
            versionInformationLab->setText(tr("Your system is the latest!"));
            systemPortraitLab->setPixmap(QPixmap(":/img/plugins/upgrade/normal.png").scaled(96,96));
            checkUpdateBtn->hide();
            allProgressBar->hide();
        }
        else {
            versionInformationLab->setText(tr("Part of the update failed!"));
            allProgressBar->hide();
        }
        QString updatetime = tr("No Information!");
        QSqlQuery queryInstall(QSqlDatabase::database("A"));
        queryInstall.exec("select * from installed order by id desc");
        while (queryInstall.next()) {
            QString statusType = queryInstall.value("keyword").toString();
            if(statusType == "" || statusType =="1") {
                updatetime = queryInstall.value("time").toString();
                break;
            }
        }
        lastRefreshTime->setText(tr("Last refresh:")+updatetime);
        lastRefreshTime->show();
        allProgressBar->hide();
    }
}

void TabWid::changeUpdateAllSlot(bool isUpdate)
{
    if (isUpdate) {
        checkUpdateBtn->setEnabled(false);
        versionInformationLab->setText(tr("Being updated..."));
    } else  {
        if(checkUpdateBtn->isEnabled() == false)
        {
            checkUpdateBtn->setText(tr("UpdateAll"));
            checkUpdateBtn->setEnabled(true);
        }
    }
}


void TabWid::waitCrucialInstalled()
{
    if(fileLockedStatus == false)
    {
        QString msg = tr("An important update is in progress, please wait.");
        //        QString msg = tr("正在进行一项重要更新，请等待。");
        updateMutual->onRequestSendDesktopNotify(msg);
        versionInformationLab->setText(msg);
        fileLockedStatus = true;
    }
}

void TabWid::getReplyFalseSlot()
{
    isConnectSourceSignal = true;
    disconnectSource();
}

void TabWid::receiveBackupStartResult(int result)
{
    switch (result) {
    case int(backuptools::backup_result::BACKUP_START_SUCCESS):
        //        versionInformationLab->setText(tr("开始备份，正在获取进度")+"...");
        versionInformationLab->setText(tr("Start backup,getting progress")+"...");
        checkUpdateBtn->start();
        checkUpdateBtn->setEnabled(false);
        return;
    case int(backuptools::backup_result::WRITE_STORAGEINFO_ADD_ITEM_FAIL):
    case int(backuptools::backup_result::WRITE_STORAGEINFO_UPDATE_ITEM_FAIL):
        //        backupMessageBox(tr("写入配置文件失败，本次更新不会备份系统！"));
        backupMessageBox(tr("Failed to write configuration file, this update will not back up the system!"));
        break;
    case int(backuptools::backup_result::BACKUP_CAPACITY_IS_NOT_ENOUGH):
        //        backupMessageBox(tr("备份空间不足，本次更新不会备份系统！"));
        backupMessageBox(tr("Insufficient backup space, this update will not backup your system!"));
        break;
    case int(backuptools::backup_result::INC_NOT_FOUND_UUID):
        //        backupMessageBox(tr("麒麟备份还原工具无法找到UUID，本次更新不会备份系统!"));
        backupMessageBox(tr("Kylin backup restore tool could not find the UUID, this update will not backup the system!"));
        break;
    default:
        backupMessageBox(tr("The backup restore partition is abnormal. You may not have a backup restore partition.For more details,see /var/log/backup.log"));
        break;
    }
}

void TabWid::whenStateIsDuing()
{
    versionInformationLab->setText(tr("Calculating Capacity..."));
    checkUpdateBtn->start();
    checkUpdateBtn->setEnabled(false);
}

bool TabWid::get_battery()
{
    QStringList users;
    int battery_value = 0;
    QDBusInterface m_interface1( "org.freedesktop.UPower",
                                 "/org/freedesktop/UPower",
                                 "org.freedesktop.UPower",
                                 QDBusConnection::systemBus() );
    if (!m_interface1.isValid()) {
        qDebug() << "电源管理器dbus接口初始化失败";
        return true;
    }

    QDBusReply<QList<QDBusObjectPath>> obj_reply = m_interface1.call("EnumerateDevices");

    if (obj_reply.isValid()) {
        for (QDBusObjectPath op : obj_reply.value())
            users << op.path();
        if (users.size()==1 || users.isEmpty()) {
            qDebug()<<"无法获取电量值,判断此电脑为台式电脑";
            return true;
        }
        foreach (QString str, users) {
            if (str == users.at(0) || str == users.at(users.size() - 1)) {
                continue ;
            }
            QDBusInterface m_interface( "org.freedesktop.UPower",
                                        str,
                                        "org.freedesktop.DBus.Properties",
                                        QDBusConnection::systemBus());

            if (!m_interface.isValid()) {
                qDebug() << "电源管理器dbus接口初始化失败";
                return true;
            }

            QDBusReply<QVariant> obj_reply = m_interface.call("Get","org.freedesktop.UPower.Device","Percentage");
            int Ele_surplus = obj_reply.value().toInt();
            battery_value += Ele_surplus;
            qDebug() << "battery value : " << Ele_surplus;
        }
        return true;
    }
    /*如果电池总电量小于50不可升级*/
    if (battery_value < 50)
        return false;
    return true;
}

void TabWid::fileLock()
{
    QDir dir("/tmp/auto-upgrade/");
    if(! dir.exists()) {
        dir.mkdir("/tmp/auto-upgrade/");//只创建一级子目录，即必须保证上级目录存在
        chmod("/tmp/auto-upgrade/",0777);
    }
    umask(0000);
    int fd = open("/tmp/auto-upgrade/ukui-control-center.lock", O_RDONLY | O_CREAT,0666);
    if (fd < 0) {
        qDebug()<<"解锁时文件锁打开异常";
        return;
    }
    flock(fd, LOCK_EX | LOCK_NB);
}

void TabWid::fileUnLock()
{
    QDir dir("/tmp/auto-upgrade/");
    if(! dir.exists()) {
        dir.mkdir("/tmp/auto-upgrade/");//只创建一级子目录，即必须保证上级目录存在
        chmod("/tmp/auto-upgrade/",0777);
    }
    umask(0000);
    int fd = open("/tmp/auto-upgrade/ukui-control-center.lock", O_RDONLY | O_CREAT,0666);
    if (fd < 0) {
        qDebug()<<"解锁时文件锁打开异常";
        return;
    }
    flock(fd, LOCK_UN);
}
