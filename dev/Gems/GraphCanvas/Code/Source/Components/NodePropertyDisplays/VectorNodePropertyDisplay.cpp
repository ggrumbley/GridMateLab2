/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include "precompiled.h"

#include <QGraphicsLinearLayout>
#include <QGraphicsSceneDragDropEvent>
#include <qgraphicsproxywidget.h>
#include <QGraphicsView>
#include <QMimeData>

#include <AzToolsFramework/UI/PropertyEditor/DHQSpinbox.hxx>

#include <Components/NodePropertyDisplays/VectorNodePropertyDisplay.h>

#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>
#include <GraphCanvas/Components/VisualBus.h>
#include <GraphCanvas/Widgets/NodePropertyBus.h>
#include <Widgets/GraphCanvasLabel.h>

namespace GraphCanvas
{
    //////////////////////
    // VectorEventFilter
    //////////////////////

    VectorEventFilter::VectorEventFilter(VectorNodePropertyDisplay* owner)
        : m_owner(owner)
    {
    }

    bool VectorEventFilter::eventFilter(QObject*, QEvent* event)
    {
        switch (event->type())
        {
        case QEvent::FocusIn:
            m_owner->OnFocusIn();
            break;
        case QEvent::FocusOut:
            m_owner->OnFocusOut();
            break;
        default:
            break;
        }

        return false;
    }

    //////////////////////////
    // ReadOnlyVectorControl
    //////////////////////////
    
    ReadOnlyVectorControl::ReadOnlyVectorControl(int index, const VectorDataInterface& dataInterface)
        : m_index(index)
        , m_dataInterface(dataInterface)
    {
        m_layout = new QGraphicsLinearLayout(Qt::Orientation::Horizontal);
        m_layout->setSpacing(0);
        m_layout->setContentsMargins(0, 0, 0, 0);
        
        m_textLabel = aznew GraphCanvasLabel();
        m_valueLabel = aznew GraphCanvasLabel();
        
        m_layout->addItem(m_textLabel);
        m_layout->addItem(m_valueLabel);

        m_textLabel->SetLabel(dataInterface.GetLabel(index));
        
        setContentsMargins(0, 0, 0, 0);
    
        setLayout(m_layout);
    }
    
    ReadOnlyVectorControl::~ReadOnlyVectorControl()
    {
    }
    
    void ReadOnlyVectorControl::RefreshStyle(const AZ::EntityId& sceneId)
    {
        AZStd::string styleName = m_dataInterface.GetElementStyle(m_index);
        m_textLabel->SetSceneStyle(sceneId, NodePropertyDisplay::CreateDisplayLabelStyle(styleName + "_text").c_str());
        m_valueLabel->SetSceneStyle(sceneId, NodePropertyDisplay::CreateDisplayLabelStyle(styleName + "_value").c_str());
    }
    
    void ReadOnlyVectorControl::UpdateDisplay()
    {
        double value = m_dataInterface.GetValue(m_index);

        AZStd::string displayValue = AZStd::string::format("%.*f%s", m_dataInterface.GetDisplayDecimalPlaces(m_index), value, m_dataInterface.GetSuffix(m_index));

        m_valueLabel->SetLabel(displayValue);
    }

    int ReadOnlyVectorControl::GetIndex() const
    {
        return m_index;
    }

    const GraphCanvasLabel* ReadOnlyVectorControl::GetTextLabel() const
    {
        return m_textLabel;
    }

    const GraphCanvasLabel* ReadOnlyVectorControl::GetValueLabel() const
    {
        return m_valueLabel;
    }

    //////////////////////////////
    // VectorNodePropertyDisplay
    //////////////////////////////
    VectorNodePropertyDisplay::VectorNodePropertyDisplay(VectorDataInterface* dataInterface)
        : m_dataInterface(dataInterface)
    {    
        m_dataInterface->RegisterDisplay(this);
        
        m_displayWidget = new QGraphicsWidget();
        m_displayWidget->setContentsMargins(0, 0, 0, 0);
        m_displayWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        QGraphicsLinearLayout* displayLayout = new QGraphicsLinearLayout(Qt::Orientation::Horizontal);
        displayLayout->setSpacing(0);
        displayLayout->setContentsMargins(0, 0, 0, 0);
        displayLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        int elementCount = dataInterface->GetElementCount();
        m_vectorDisplays.reserve(elementCount);
        for (int i=0; i < elementCount; ++i)
        {
            m_vectorDisplays.push_back(aznew ReadOnlyVectorControl(i, (*dataInterface)));
            displayLayout->addItem(m_vectorDisplays.back());
        }

        m_displayWidget->setLayout(displayLayout);
        
        m_disabledLabel = aznew GraphCanvasLabel();
        m_proxyWidget = new QGraphicsProxyWidget();
        
        m_proxyWidget->setFlag(QGraphicsItem::ItemIsFocusable, true);
        m_proxyWidget->setFocusPolicy(Qt::StrongFocus);
        m_proxyWidget->setAcceptDrops(false);
        
        m_propertyVectorCtrl = aznew AzToolsFramework::PropertyVectorCtrl(nullptr, elementCount);
        m_propertyVectorCtrl->setProperty("HasNoWindowDecorations", true);
        m_propertyVectorCtrl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        AzToolsFramework::VectorElement** elements = m_propertyVectorCtrl->getElements();
        
        for (int i=0; i < elementCount; ++i)
        {
            m_propertyVectorCtrl->setLabel(i, m_dataInterface->GetLabel(i));
            m_propertyVectorCtrl->setMinimum(m_dataInterface->GetMinimum(i));
            m_propertyVectorCtrl->setMaximum(m_dataInterface->GetMaximum(i));
            m_propertyVectorCtrl->setDecimals(m_dataInterface->GetDecimalPlaces(i));
            m_propertyVectorCtrl->setDisplayDecimals(m_dataInterface->GetDisplayDecimalPlaces(i));
            m_propertyVectorCtrl->setSuffix(m_dataInterface->GetSuffix(i));

            elements[i]->GetSpinBox()->installEventFilter(aznew VectorEventFilter(this));
        }
        
        QObject::connect(m_propertyVectorCtrl, &AzToolsFramework::PropertyVectorCtrl::valueAtIndexChanged, [this](int elementIndex, double newValue) { SubmitValue(elementIndex, newValue); });
        
        m_proxyWidget->setWidget(m_propertyVectorCtrl);

        RegisterShortcutDispatcher(m_propertyVectorCtrl);
    }
    
    VectorNodePropertyDisplay::~VectorNodePropertyDisplay()
    {
        NodePropertiesRequestBus::Event(GetNodeId(), &NodePropertiesRequests::UnlockEditState, this);

        delete m_dataInterface;
        m_dataInterface = nullptr;
        
        delete m_disabledLabel;
        delete m_displayWidget;
        delete m_proxyWidget;
    }
    
    void VectorNodePropertyDisplay::RefreshStyle()
    {
        AZ::EntityId sceneId = GetSceneId();

        AZStd::string elementStyle = m_dataInterface->GetStyle();

        m_styleHelper.SetScene(sceneId);
        m_styleHelper.SetStyle(NodePropertyDisplay::CreateDisplayLabelStyle(elementStyle).c_str());
        m_disabledLabel->SetSceneStyle(GetSceneId(), NodePropertyDisplay::CreateDisabledLabelStyle(elementStyle).c_str());

        QColor backgroundColor = m_styleHelper.GetAttribute(GraphCanvas::Styling::Attribute::BackgroundColor, QColor(0, 0, 0, 0));

        m_displayWidget->setAutoFillBackground(true);

        QPalette palette = m_displayWidget->palette();
        palette.setColor(QPalette::ColorRole::Window, backgroundColor);
        m_displayWidget->setPalette(palette);

        qreal spacing = static_cast<QGraphicsLinearLayout*>(m_displayWidget->layout())->spacing();

        // Start off with - spacing to make the iteration logic cleaner.
        qreal elementWidth = -spacing;
        qreal elementHeight = m_styleHelper.GetAttribute(GraphCanvas::Styling::Attribute::Height, 0);

        const qreal k_sizingConstraint = 200;
        
        for (auto control : m_vectorDisplays)
        {
            control->RefreshStyle(sceneId);

            QSizeF maximumSize = control->maximumSize();

            // Maximum size might be stupidly large, which will cause an error
            // from Qt as it tries to overly allocate space for something.
            //
            // As such we want to put an upper limit on this that is large but not unreasonable.
            // Can't really do this at the element level since it messes with the styling
            // when I set it. So instead we'll do it here.
            elementWidth += AZ::GetMin(maximumSize.width(), k_sizingConstraint) + spacing;
            elementHeight = AZStd::GetMax(elementHeight, AZ::GetMin(k_sizingConstraint, maximumSize.height()));

            const Styling::StyleHelper& styleHelper = control->GetTextLabel()->GetStyleHelper();

            // Get the font stylesheet from the StyleHelper and pass it along to the PropertyVector
            m_propertyVectorCtrl->setLabelStyle(control->GetIndex(), styleHelper.GetFontStyleSheet().toUtf8().data());
        }

        m_displayWidget->setMinimumSize(elementWidth, elementHeight);
        m_displayWidget->setPreferredSize(elementWidth, elementHeight);
        m_displayWidget->setMaximumSize(elementWidth, elementHeight);
        m_displayWidget->adjustSize();

        m_propertyVectorCtrl->setMinimumSize(elementWidth, elementHeight);
        m_propertyVectorCtrl->setMaximumSize(elementWidth, elementHeight);
        m_propertyVectorCtrl->adjustSize();
    }
    
    void VectorNodePropertyDisplay::UpdateDisplay()
    {
        for (auto control : m_vectorDisplays)
        {
            control->UpdateDisplay();
        }

        for (int i = 0; i < m_vectorDisplays.size(); ++i)
        {
            m_propertyVectorCtrl->setValuebyIndex(m_dataInterface->GetValue(i), i);
        }

        m_proxyWidget->update();
    }
    
    QGraphicsLayoutItem* VectorNodePropertyDisplay::GetDisabledGraphicsLayoutItem() const
    {
        return m_disabledLabel;
    }
    
    QGraphicsLayoutItem* VectorNodePropertyDisplay::GetDisplayGraphicsLayoutItem() const
    {
        return m_displayWidget;
    }
    
    QGraphicsLayoutItem* VectorNodePropertyDisplay::GetEditableGraphicsLayoutItem() const
    {
        return m_proxyWidget;
    }    
    
    void VectorNodePropertyDisplay::OnFocusIn()
    {
        NodePropertiesRequestBus::Event(GetNodeId(), &NodePropertiesRequests::LockEditState, this);
        TryAndSelectNode();
    }
    
    void VectorNodePropertyDisplay::SubmitValue(int elementIndex, double newValue)
    {
        m_dataInterface->SetValue(elementIndex, newValue);
        UpdateDisplay();
    }

    void VectorNodePropertyDisplay::OnFocusOut()
    {
        NodePropertiesRequestBus::Event(GetNodeId(), &NodePropertiesRequests::UnlockEditState, this);
    }

#include <Source/Components/NodePropertyDisplays/VectorNodePropertyDisplay.moc>
}