/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMouseHandler class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMouseHandler_h___
#define ___UIMouseHandler_h___

/* Global includes */
#include <QObject>
#include <QPoint>
#include <QMap>

/* Local includes */
#include "UIMachineDefs.h"

/* Global forwards */
class QWidget;

/* Local forwards */
class CSession;
class UISession;
class UIMachineLogic;
class UIMachineView;

/* Delegate to control VM mouse functionality: */
class UIMouseHandler : public QObject
{
    Q_OBJECT;

public:

    /* Factory functions to create/destroy mouse-handler: */
    static UIMouseHandler* create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType);
    static void destroy(UIMouseHandler *pMouseHandler);

    /* Registers/Remove new machine-view: */
    void addMachineView(ulong uViewIndex, UIMachineView *pMachineView);
    void delMachineView(ulong uViewIndex);

    /* Commands to capture/release mouse: */
    void captureMouse(ulong uScreenId);
    void releaseMouse();

    /* Setter for mouse-integration feature: */
    void setMouseIntegrationEnabled(bool fEnabled);

    /* Current mouse state: */
    int mouseState() const;

#ifdef Q_WS_MAC
    /* Enable mouse event compression if we leave the VM view.
     * This is necessary for having smooth resizing of the VM/other windows.
     * Disable mouse event compression if we enter the VM view.
     * So all mouse events are registered in the VM.
    void setMouseCoalescingEnabled(bool fOn);
#endif /* Q_WS_MAC */

signals:

    /* Notifies listeners about mouse state-change: */
    void mouseStateChanged(int iNewState);

protected slots:

    /* Machine state-change handler: */
    virtual void sltMachineStateChanged();

    /* Mouse capability-change handler: */
    virtual void sltMouseCapabilityChanged();

    /* Mouse pointer-shape-change handler: */
    virtual void sltMousePointerShapeChanged();

protected:

    /* Mouse-handler constructor/destructor: */
    UIMouseHandler(UIMachineLogic *pMachineLogic);
    virtual ~UIMouseHandler();

    /* Getters: */
    UIMachineLogic* machineLogic() const;
    UISession* uisession() const;
    CSession& session() const;

    /* Event handler for registered machine-view(s): */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Separate function to handle most of existing mouse-events: */
    bool mouseEvent(int iEventType, ulong uScreenId,
                    const QPoint &relativePos, const QPoint &globalPos,
                    Qt::MouseButtons mouseButtons,
                    int wheelDelta, Qt::Orientation wheelDirection);

#ifdef Q_WS_WIN
    /* This method is actually required only because under win-host
     * we do not really grab the mouse in case of capturing it: */
    void updateMouseCursorClipping();
#endif /* Q_WS_WIN */

    /* Machine logic parent: */
    UIMachineLogic *m_pMachineLogic;

    /* Registered machine-view(s): */
    QMap<ulong, UIMachineView*> m_views;
    /* Registered machine-view-viewport(s): */
    QMap<ulong, QWidget*> m_viewports;

    /* Other mouse variables: */
    QPoint m_lastMousePos;
    QPoint m_capturedMousePos;
    int m_iLastMouseWheelDelta;
    int m_iMouseCaptureViewIndex;
};

#endif // !___UIMouseHandler_h___

