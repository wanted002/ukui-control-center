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
#include "ukmedia_sound_effects_widget.h"
#include <QHBoxLayout>
#include <QFile>
#include <QVBoxLayout>
#include <QDebug>

UkuiMessageBox::UkuiMessageBox()
{
}

UkmediaSoundEffectsWidget::UkmediaSoundEffectsWidget(QWidget *parent) : QWidget(parent)
{
    m_pThemeWidget = new QFrame(this);
    m_pAlertSoundWidget = new QFrame(this);
    m_pAlertSoundSwitchWidget = new QFrame(this);
    m_pAlertSoundVolumeWidget = new QFrame(this);
    m_pStartupMusicWidget = new QFrame(this);
    m_pPoweroffMusicWidget = new QFrame(this);
    m_pLagoutWidget = new QFrame(this);
    m_pWakeupMusicWidget = new QFrame(this);
    m_pVolumeChangeWidget = new QFrame(this);

    m_pThemeWidget->setFrameShape(QFrame::Shape::Box);
    m_pAlertSoundWidget->setFrameShape(QFrame::Shape::Box);
    m_pStartupMusicWidget->setFrameShape(QFrame::Shape::Box);
    m_pLagoutWidget->setFrameShape(QFrame::Shape::Box);
    m_pAlertSoundSwitchWidget->setFrameShape(QFrame::Shape::Box);
    m_pAlertSoundVolumeWidget->setFrameShape(QFrame::Shape::Box);
    m_pWakeupMusicWidget->setFrameShape(QFrame::Shape::Box);
    m_pVolumeChangeWidget->setFrameShape(QFrame::Shape::Box);
    m_pPoweroffMusicWidget->setFrameShape(QFrame::Shape::Box);

    m_pSoundEffectLabel = new TitleLabel(this);
    m_pSoundEffectLabel->setText(tr("System Sound"));
    m_pSoundEffectLabel->setStyleSheet("QLabel{color: palette(windowText);}");
    //~ contents_path /audio/Sound Theme
    m_pSoundThemeLabel = new QLabel(tr("Sound Theme"),m_pThemeWidget);
    m_pSoundThemeCombobox = new QComboBox(m_pThemeWidget);
    //~ contents_path /audio/Alert Sound
    m_pShutdownlabel = new QLabel(tr("Alert Sound"),m_pAlertSoundWidget);
    m_pAlertSoundCombobox = new QComboBox(m_pAlertSoundWidget);
    m_pAlertSoundLabel = new QLabel(tr("Alert Volume"),m_pAlertSoundVolumeWidget);
    m_pAlertVolumeLabel = new QLabel(m_pAlertSoundVolumeWidget);
    m_pAlertSlider = new UkmediaVolumeSlider(m_pAlertSoundVolumeWidget);
    m_pAlertIconBtn = new UkuiButtonDrawSvg(m_pAlertSoundVolumeWidget);
    m_pAlertSlider->setRange(0,100);
    QSize iconSize(24,24);
    m_pAlertIconBtn->setFocusPolicy(Qt::NoFocus);
//    m_pAlertIconBtn->setFlat(true);
    m_pAlertIconBtn->setFixedSize(24,24);
    m_pAlertIconBtn->setIconSize(iconSize);
    m_pAlertSlider->setOrientation(Qt::Horizontal);
    QPalette paleteAppIcon = m_pAlertIconBtn->palette();
    paleteAppIcon.setColor(QPalette::Highlight,Qt::transparent);
    paleteAppIcon.setBrush(QPalette::Button,QBrush(QColor(1,1,1,0)));
    m_pAlertIconBtn->setPalette(paleteAppIcon);
//    m_pAlertIconBtn->setProperty("useIconHighlightEffect",true);
//    m_pAlertIconBtn->setProperty("iconHighlightEffectMode",true);

    //~ contents_path /audio/Beep Switch
    m_pAlertSoundSwitchLabel = new QLabel(tr("Beep Switch"),m_pAlertSoundSwitchWidget);
    //~ contents_path /audio/Poweroff Music
    m_pPoweroffMusicLabel = new QLabel(tr("Poweroff Music"),m_pPoweroffMusicWidget);
    //~ contents_path /audio/Startup Music
    m_pStartupMusicLabel = new QLabel(tr("Startup Music"),m_pStartupMusicWidget);
    //~ contents_path /audio/Wakeup Music
    m_pWakeupMusicLabel = new QLabel(tr("Wakeup Music"),m_pWakeupMusicWidget);
    //~ contents_path /audio/Volume Change
    m_pVolumeChangeLabel = new QLabel(tr("Volume Change"),m_pVolumeChangeWidget);
    //~ contents_path /audio/Logout Music
    m_pLagoutLabel = new QLabel(tr("Logout Music"),m_pLagoutWidget);

    m_pLagoutCombobox = new QComboBox(m_pLagoutWidget);
    m_pStartupButton = new SwitchButton(m_pStartupMusicWidget);
    m_pLogoutButton = new SwitchButton(m_pLagoutWidget);
    m_pWakeupMusicButton = new SwitchButton(m_pWakeupMusicWidget);
    m_pPoweroffButton = new SwitchButton(m_pPoweroffMusicWidget);
    m_pVolumeChangeCombobox = new QComboBox(m_pVolumeChangeWidget);

    m_pAlertSoundSwitchButton = new SwitchButton(m_pAlertSoundSwitchWidget);

    //设置大小
    m_pThemeWidget->setMinimumSize(550,50);
    m_pThemeWidget->setMaximumSize(960,50);
    m_pAlertSoundWidget->setMinimumSize(550,50);
    m_pAlertSoundWidget->setMaximumSize(960,50);
    m_pStartupMusicWidget->setMinimumSize(550,50);
    m_pStartupMusicWidget->setMaximumSize(960,50);
    m_pLagoutWidget->setMinimumSize(550,50);
    m_pLagoutWidget->setMaximumSize(960,50);
    m_pAlertSoundSwitchWidget->setMinimumSize(550,50);
    m_pAlertSoundSwitchWidget->setMaximumSize(960,50);
    m_pWakeupMusicWidget->setMinimumSize(550,50);
    m_pWakeupMusicWidget->setMaximumSize(960,50);
    m_pVolumeChangeWidget->setMinimumSize(550,50);
    m_pVolumeChangeWidget->setMaximumSize(960,50);
    m_pPoweroffMusicWidget->setMinimumSize(550,50);
    m_pPoweroffMusicWidget->setMaximumSize(960,50);
    m_pAlertSoundVolumeWidget->setMinimumSize(550,50);
    m_pAlertSoundVolumeWidget->setMaximumSize(960,50);

    m_pSoundEffectLabel->setFixedSize(150,32);
    m_pSoundThemeLabel->setFixedSize(150,32);
    m_pShutdownlabel->setFixedSize(150,32);
    m_pLagoutLabel->setFixedSize(150,32);
    m_pWakeupMusicLabel->setFixedSize(150,32);
    m_pVolumeChangeLabel->setFixedSize(150,32);
    m_pPoweroffMusicLabel->setFixedSize(150,32);

    m_pSoundThemeCombobox->setMinimumSize(50,32);
    m_pSoundThemeCombobox->setMaximumSize(900,32);

    m_pAlertSoundCombobox->setMinimumSize(50,32);
    m_pAlertSoundCombobox->setMaximumSize(900,32);
    m_pLagoutCombobox->setMinimumSize(50,32);
    m_pLagoutCombobox->setMaximumSize(900,32);

    m_pVolumeChangeCombobox->setMinimumSize(50,32);
    m_pVolumeChangeCombobox->setMaximumSize(900,32);

    //添加布局
    QHBoxLayout *themeLayout = new QHBoxLayout(m_pThemeWidget);
    themeLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    themeLayout->addWidget(m_pSoundThemeLabel);
    themeLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    themeLayout->addWidget(m_pSoundThemeCombobox);
    themeLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    themeLayout->setSpacing(0);
    m_pThemeWidget->setLayout(themeLayout);
    m_pThemeWidget->layout()->setContentsMargins(0,0,0,0);

    QHBoxLayout *AlertLayout = new QHBoxLayout(m_pAlertSoundWidget);
    AlertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    AlertLayout->addWidget(m_pShutdownlabel);
    AlertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    AlertLayout->addWidget(m_pAlertSoundCombobox);
    AlertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    AlertLayout->setSpacing(0);
    m_pAlertSoundWidget->setLayout(AlertLayout);
    m_pAlertSoundWidget->layout()->setContentsMargins(0,0,0,0);

    //开机音乐设置开关
    QHBoxLayout *startupLayout = new QHBoxLayout(m_pStartupMusicWidget);
    startupLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    startupLayout->addWidget(m_pStartupMusicLabel);
    startupLayout->addWidget(m_pStartupButton);
    startupLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    startupLayout->setSpacing(0);
    m_pStartupMusicWidget->setLayout(startupLayout);
    m_pStartupMusicWidget->layout()->setContentsMargins(0,0,0,0);

    //注销提示音布局
    QHBoxLayout *lagoutLayout = new QHBoxLayout(m_pLagoutWidget);
    lagoutLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    lagoutLayout->addWidget(m_pLagoutLabel);
    lagoutLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Expanding));
//    lagoutLayout->addWidget(m_pLagoutCombobox);
    lagoutLayout->addWidget(m_pLogoutButton);
    lagoutLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    lagoutLayout->setSpacing(0);
    m_pLagoutWidget->setLayout(lagoutLayout);
    m_pLagoutWidget->layout()->setContentsMargins(0,0,0,0);
//    m_pLagoutWidget->setVisible(false);
    m_pLagoutCombobox->setVisible(false);

    //提示音开关布局
    QHBoxLayout *alertSoundLayout = new QHBoxLayout(m_pAlertSoundSwitchWidget);
    alertSoundLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertSoundLayout->addWidget(m_pAlertSoundSwitchLabel);
    alertSoundLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Expanding));
//    lagoutLayout->addWidget(m_pLagoutCombobox);
    alertSoundLayout->addWidget(m_pAlertSoundSwitchButton);
    alertSoundLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertSoundLayout->setSpacing(0);
    m_pAlertSoundSwitchWidget->setLayout(alertSoundLayout);
    m_pAlertSoundSwitchWidget->layout()->setContentsMargins(0,0,0,0);
    //提示音大小
    QHBoxLayout *alertLayout = new QHBoxLayout(m_pAlertSoundVolumeWidget);
    m_pAlertSoundLabel->setFixedSize(150,32);
    m_pAlertSlider->setFixedHeight(20);
    m_pAlertVolumeLabel->setFixedSize(40,24);
    alertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertLayout->addWidget(m_pAlertSoundLabel);
    alertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertLayout->addWidget(m_pAlertIconBtn);
    alertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertLayout->addWidget(m_pAlertSlider);
    alertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertLayout->addWidget(m_pAlertVolumeLabel);
    alertLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    alertLayout->setSpacing(0);
    m_pAlertSoundVolumeWidget->setLayout(alertLayout);
    m_pAlertSoundVolumeWidget->layout()->setContentsMargins(0,0,0,0);

    //窗口关闭提示音
    QHBoxLayout *wakeupMusicLayout = new QHBoxLayout(m_pWakeupMusicWidget);
    wakeupMusicLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    wakeupMusicLayout->addWidget(m_pWakeupMusicLabel);
    wakeupMusicLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Expanding));
    wakeupMusicLayout->addWidget(m_pWakeupMusicButton);
    wakeupMusicLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    wakeupMusicLayout->setSpacing(0);
    m_pWakeupMusicWidget->setLayout(wakeupMusicLayout);
    m_pWakeupMusicWidget->layout()->setContentsMargins(0,0,0,0);

    //音量改变提示音
    QHBoxLayout *volumeChangedLayout = new QHBoxLayout(m_pVolumeChangeWidget);
    volumeChangedLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    volumeChangedLayout->addWidget(m_pVolumeChangeLabel);
    volumeChangedLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    volumeChangedLayout->addWidget(m_pVolumeChangeCombobox);
    volumeChangedLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    volumeChangedLayout->setSpacing(0);
    m_pVolumeChangeWidget->setLayout(volumeChangedLayout);
    m_pVolumeChangeWidget->layout()->setContentsMargins(0,0,0,0);
    //关机提示音
    QHBoxLayout *poweroffLayout = new QHBoxLayout(m_pPoweroffMusicWidget);
    poweroffLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    poweroffLayout->addWidget(m_pPoweroffMusicLabel);
    poweroffLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Expanding));
    poweroffLayout->addWidget(m_pPoweroffButton);
    poweroffLayout->addItem(new QSpacerItem(16,20,QSizePolicy::Fixed));
    poweroffLayout->setSpacing(0);
    m_pPoweroffMusicWidget->setLayout(poweroffLayout);
    m_pPoweroffMusicWidget->layout()->setContentsMargins(0,0,0,0);

    //进行整体布局
    m_pSoundLayout = new QVBoxLayout(this);
    m_pSoundLayout->addWidget(m_pSoundEffectLabel);
    m_pSoundLayout->addItem(new QSpacerItem(16,8,QSizePolicy::Fixed));
    m_pSoundLayout->addWidget(m_pStartupMusicWidget);
    m_pSoundLayout->addItem(new QSpacerItem(16,1,QSizePolicy::Fixed));
    m_pSoundLayout->addWidget(m_pPoweroffMusicWidget);
    m_pSoundLayout->addItem(new QSpacerItem(16,1,QSizePolicy::Fixed));
    m_pSoundLayout->addWidget(m_pLagoutWidget);
    m_pSoundLayout->addItem(new QSpacerItem(16,1,QSizePolicy::Fixed));
    m_pSoundLayout->addWidget(m_pWakeupMusicWidget);

    m_pSoundLayout->addItem(new QSpacerItem(16,6,QSizePolicy::Fixed));
    m_pSoundLayout->addWidget(m_pAlertSoundSwitchWidget);
//    m_pSoundLayout->addWidget(m_pAlertSoundVolumeWidget);
    m_pSoundLayout->addItem(new QSpacerItem(16,7,QSizePolicy::Fixed));
    m_pSoundLayout->addWidget(m_pThemeWidget);
    m_pSoundLayout->addWidget(m_pAlertSoundWidget);
//    vLayout->addWidget(m_pWindowClosedWidget);
    m_pSoundLayout->addWidget(m_pVolumeChangeWidget);
//    vLayout->addWidget(m_pSettingSoundWidget);
    this->setLayout(m_pSoundLayout);
    m_pSoundLayout->setSpacing(1);
    this->layout()->setContentsMargins(0,0,0,0);
    m_pAlertSoundVolumeWidget->hide();
}

UkmediaSoundEffectsWidget::~UkmediaSoundEffectsWidget()
{

}
