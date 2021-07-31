/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "keyboardcontrol.h"
#include "ui_keyboardcontrol.h"

#include <QGSettings>
#include <QProcess>

#include <QDebug>
#include <QMouseEvent>

#define KEYBOARD_SCHEMA "org.ukui.peripherals-keyboard"
#define REPEAT_KEY "repeat"
#define DELAY_KEY "delay"
#define RATE_KEY "rate"

#define KBD_LAYOUTS_SCHEMA "org.mate.peripherals-keyboard-xkb.kbd"
#define KBD_LAYOUTS_KEY "layouts"

#define CC_KEYBOARD_OSD_SCHEMA "org.ukui.control-center.osd"
#define CC_KEYBOARD_OSD_KEY "show-lock-tip"

KeyboardControl::KeyboardControl() : mFirstLoad(true)
{
    pluginName = tr("Keyboard");
    pluginType = DEVICES;
}

KeyboardControl::~KeyboardControl()
{
    if (!mFirstLoad) {
        delete ui;
        ui = nullptr;
        if (settingsCreate) {
//            delete kbdsettings;
            delete settings;
            settings = nullptr;
        }
    }
}

QString KeyboardControl::get_plugin_name() {
    return pluginName;
}

int KeyboardControl::get_plugin_type() {
    return pluginType;
}

QWidget *KeyboardControl::get_plugin_ui() {
    if (mFirstLoad) {
        ui = new Ui::KeyboardControl;
        pluginWidget = new QWidget;
        pluginWidget->setAttribute(Qt::WA_DeleteOnClose);
        ui->setupUi(pluginWidget);

        mFirstLoad = false;
        settingsCreate = false;

        setupStylesheet();
        setupComponent();

        // 初始化键盘通用设置GSettings
        const QByteArray id(KEYBOARD_SCHEMA);
        // 初始化键盘布局GSettings
        // const QByteArray idd(KBD_LAYOUTS_SCHEMA);
        // 初始化按键提示GSettings
        const QByteArray iid(CC_KEYBOARD_OSD_SCHEMA);
        // 控制面板自带GSettings，不再判断是否安装
        osdSettings = new QGSettings(iid);

        if (QGSettings::isSchemaInstalled(id)){
            settingsCreate = true;

//            kbdsettings = new QGSettings(idd);
            settings = new QGSettings(id);

            //构建布局管理器对象
            layoutmanagerObj = new KbdLayoutManager(pluginWidget);

            setupConnect();
            initGeneralStatus();

            rebuildLayoutsComBox();
        }

    }
    return pluginWidget;
}

void KeyboardControl::plugin_delay_control() {

}

const QString KeyboardControl::name() const {

    return QStringLiteral("keyboard");
}

void KeyboardControl::setupStylesheet(){

    //~ contents_path /keyboard/Enable repeat key
    ui->enableLabel->setText(tr("Enable repeat key"));
    //~ contents_path /keyboard/Delay
    ui->delayLabel->setText(tr("Delay"));
    //~ contents_path /keyboard/Speed
    ui->speedLabel->setText(tr("Speed"));
    //~ contents_path /keyboard/Input characters to test the repetition effect:
    ui->repeatLabel->setText(tr("Input characters to test the repetition effect:"));
    //~ contents_path /keyboard/Tip of keyboard
    ui->tipLabel->setText(tr("Tip of keyboard"));
    ui->layoutLabel->setText(tr("Keyboard layout"));
    //~ contents_path /keyboard/Input Settings
    ui->title2Label->setText(tr("Input Settings"));

}

void KeyboardControl::setupComponent(){

//    addWgt = new HoverWidget("");
//    addWgt->setObjectName("addwgt");
//    addWgt->setMinimumSize(QSize(580, 50));
//    addWgt->setMaximumSize(QSize(960, 50));
//    addWgt->setStyleSheet("HoverWidget#addwgt{background: palette(button); border-radius: 4px;}HoverWidget:hover:!pressed#addwgt{background: #3D6BE5; border-radius: 4px;}");


    ui->layoutFrame_0->hide();
    ui->addLytWidget->hide();

//    QHBoxLayout *addLyt = new QHBoxLayout;

//    QLabel * iconLabel = new QLabel();
//    QLabel * textLabel = new QLabel(tr("Install layouts"));
//    QPixmap pixgray = ImageUtil::loadSvg(":/img/titlebar/add.svg", "black", 12);
//    iconLabel->setPixmap(pixgray);
//    addLyt->addWidget(iconLabel);
//    addLyt->addWidget(textLabel);
//    addLyt->addStretch();
//    addWgt->setLayout(addLyt);

    // 悬浮改变Widget状态
//    connect(addWgt, &HoverWidget::enterWidget, this, [=](QString mname) {
//        Q_UNUSED(mname);
//        QPixmap pixgray = ImageUtil::loadSvg(":/img/titlebar/add.svg", "white", 12);
//        iconLabel->setPixmap(pixgray);
//        textLabel->setStyleSheet("color: palette(base);");
//    });

    // 还原状态
//    connect(addWgt, &HoverWidget::leaveWidget, this, [=](QString mname) {
//        Q_UNUSED(mname);
//        QPixmap pixgray = ImageUtil::loadSvg(":/img/titlebar/add.svg", "black", 12);
//        iconLabel->setPixmap(pixgray);
//        textLabel->setStyleSheet("color: palette(windowText);");
//    });

//    ui->addLyt->addWidget(addWgt);

    // 隐藏未开发功能
    ui->repeatFrame_5->hide();

    // 重复输入开关按钮
    keySwitchBtn = new SwitchButton(pluginWidget);
    ui->enableHorLayout->addWidget(keySwitchBtn);

    // 按键提示开关按钮
    tipKeyboardSwitchBtn = new SwitchButton(pluginWidget);
    ui->tipKeyboardHorLayout->addWidget(tipKeyboardSwitchBtn);

    // 小键盘开关按钮
    numLockSwitchBtn = new SwitchButton(pluginWidget);
    ui->numLockHorLayout->addWidget(numLockSwitchBtn);
}

void KeyboardControl::setupConnect(){
    connect(keySwitchBtn, &SwitchButton::checkedChanged, this, [=](bool checked) {
        setKeyboardVisible(checked);
        settings->set(REPEAT_KEY, checked);
    });

    connect(ui->delayHorSlider, &QSlider::valueChanged, this, [=](int value) {
        settings->set(DELAY_KEY, value);
    });

    connect(ui->speedHorSlider, &QSlider::valueChanged, this, [=](int value) {
        settings->set(RATE_KEY, value);
    });

    connect(settings,&QGSettings::changed,this,[=](const QString &key) {
       if(key == "rate") {
           ui->speedHorSlider->setValue(settings->get(RATE_KEY).toInt());
       } else if(key == "repeat") {
           keySwitchBtn->setChecked(settings->get(REPEAT_KEY).toBool());
           setKeyboardVisible(keySwitchBtn->isChecked());
       } else if(key == "delay") {
           ui->delayHorSlider->setValue(settings->get(DELAY_KEY).toInt());
       }
    });

    connect(osdSettings,&QGSettings::changed,this,[=](const QString &key) {
       if(key == "showLockTip") {
           tipKeyboardSwitchBtn->blockSignals(true);
           tipKeyboardSwitchBtn->setChecked(osdSettings->get(CC_KEYBOARD_OSD_KEY).toBool());
           tipKeyboardSwitchBtn->blockSignals(false);
       }
    });

//    connect(addWgt, &HoverWidget::widgetClicked, this, [=](QString mname) {
//        Q_UNUSED(mname);
//        KbdLayoutManager * templayoutManager = new KbdLayoutManager;
//        templayoutManager->exec();
//    });

//    connect(ui->resetBtn, &QPushButton::clicked, this, [=] {
//        kbdsettings->reset(KBD_LAYOUTS_KEY);
//        if ("zh_CN" == QLocale::system().name()) {
//            kbdsettings->set(KBD_LAYOUTS_KEY, "cn");
//        } else {
//            kbdsettings->set(KBD_LAYOUTS_KEY, "us");
//        }
//    });

    connect(ui->inputSettingsBtn, &QPushButton::clicked, this, [=]{
        QProcess process;
        process.startDetached("fcitx-config-gtk3");
    });

//    connect(kbdsettings, &QGSettings::changed, [=](QString key) {
//        if (key == KBD_LAYOUTS_KEY)
//            rebuildLayoutsComBox();
//    });

    connect(tipKeyboardSwitchBtn, &SwitchButton::checkedChanged, this, [=](bool checked) {
        osdSettings->set(CC_KEYBOARD_OSD_KEY, checked);
    });

//#if QT_VERSION <= QT_VERSION_CHECK(5, 12, 0)
//    connect(ui->layoutsComBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int index){
//#else
//    connect(ui->layoutsComBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
//#endif
//        QStringList layoutsList;
//        layoutsList.append(ui->layoutsComBox->currentData(Qt::UserRole).toString());
//        for (int i = 0; i < ui->layoutsComBox->count(); i++){
//            QString id = ui->layoutsComBox->itemData(i, Qt::UserRole).toString();
//            if (i != index) //跳过当前item
//                layoutsList.append(id);
//        }
//        kbdsettings->set(KBD_LAYOUTS_KEY, layoutsList);
//    });
}

void KeyboardControl::initGeneralStatus() {
    //设置按键重复状态
    keySwitchBtn->setChecked(settings->get(REPEAT_KEY).toBool());
    setKeyboardVisible(keySwitchBtn->isChecked());

    //设置按键重复的延时
    ui->delayHorSlider->setValue(settings->get(DELAY_KEY).toInt());

    //设置按键重复的速度
    ui->speedHorSlider->setValue(settings->get(RATE_KEY).toInt());

    tipKeyboardSwitchBtn->blockSignals(true);
    tipKeyboardSwitchBtn->setChecked(osdSettings->get(CC_KEYBOARD_OSD_KEY).toBool());
    tipKeyboardSwitchBtn->blockSignals(false);

}

void KeyboardControl::rebuildLayoutsComBox() {
//    QStringList layouts = kbdsettings->get(KBD_LAYOUTS_KEY).toStringList();
//    ui->layoutsComBox->blockSignals(true);
    //清空键盘布局下拉列表
//    ui->layoutsComBox->clear();

    //重建键盘布局下拉列表
//    for (QString layout : layouts) {
//        ui->layoutsComBox->addItem(layoutmanagerObj->kbd_get_description_by_id(const_cast<const char *>(layout.toLatin1().data())), layout);
//    }
//    ui->layoutsComBox->blockSignals(false);
//    if (0 == ui->layoutsComBox->count()) {
//        ui->layoutsComBox->setVisible(false);
//    } else {
//        ui->layoutsComBox->setVisible(true);
//    }
}

void KeyboardControl::setKeyboardVisible(bool checked) {
    ui->repeatFrame_1->setVisible(checked);
    ui->repeatFrame_2->setVisible(checked);
    ui->repeatFrame_3->setVisible(checked);
}
