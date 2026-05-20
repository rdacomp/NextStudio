/*
  ==============================================================================

    AutomatableSlider.cpp
    Created: 15 Jan 2026
    Author:  NextStudio

  ==============================================================================
*/

#include "UI/Controls/AutomatableSlider.h"
#include "Utilities/NextLookAndFeel.h"

namespace
{
constexpr int removeModifierMenuBaseId = 1;
constexpr int addAutomationLaneMenuId = 2000;
constexpr int clearAutomationMenuId = 2001;
constexpr int midiLearnMenuId = 2100;
constexpr int removeMidiMappingMenuId = 2101;
constexpr double midiLearnTimeoutMs = 10000.0;

juce::String getControllerDescription(int controllerId, int channel)
{
    const auto channelText = juce::String{"Channel "} + juce::String(channel);

    if (controllerId >= 0x40000)
        return "Channel Pressure (" + channelText + ")";

    if (controllerId >= 0x30000)
        return "RPN #" + juce::String(controllerId & 0x7fff) + " (" + channelText + ")";

    if (controllerId >= 0x20000)
        return "NRPN #" + juce::String(controllerId & 0x7fff) + " (" + channelText + ")";

    if (controllerId >= 0x10000)
    {
        const auto cc = controllerId & 0x7f;

        auto label = "CC " + juce::String(cc);
        const juce::String controllerName{juce::MidiMessage::getControllerName(cc)};

        if (controllerName.isNotEmpty())
            label << " (" << controllerName << ")";

        return label + " - " + channelText;
    }

    return "MIDI Controller";
}
}

struct AutomatableSliderComponent::MidiLearnListener final : private te::MidiLearnState::Listener
{
    MidiLearnListener(AutomatableSliderComponent &ownerIn, te::MidiLearnState &state)
        : te::MidiLearnState::Listener(state)
        , owner(ownerIn)
    {
    }

    void midiLearnStatusChanged(bool isActive) override { owner.handleMidiLearnStatusChanged(isActive); }
    void midiLearnAssignmentChanged(te::MidiLearnState::ChangeType changeType) override { owner.handleMidiLearnAssignmentChanged(changeType); }

    AutomatableSliderComponent &owner;
};

struct AutomatableSliderComponent::MidiMappingSnapshot final
{
    struct Entry
    {
        te::AutomatableParameter::Ptr parameter;
        int controllerId = -1;
        int channel = -1;
    };

    void capture(te::ParameterControlMappings &mappings)
    {
        entries.clear();

        for (int row = 0; row < mappings.getNumControllerIDs(); ++row)
        {
            auto mapping = mappings.getMappingForRow(row);

            if (mapping.parameter != nullptr && mapping.controllerID != 0)
                entries.push_back({mapping.parameter, mapping.controllerID, mapping.channelID});
        }
    }

    void restore(te::Edit &edit, te::ParameterControlMappings &mappings) const
    {
        for (int row = mappings.getNumControllerIDs(); --row >= 0;)
            mappings.removeMapping(row);

        mappings.saveToEdit();

        if (!entries.empty())
        {
            auto *undoManager = &edit.getUndoManager();
            auto state = edit.state.getOrCreateChildWithName(te::IDs::CONTROLLERMAPPINGS, undoManager);

            for (const auto &entry : entries)
            {
                if (entry.parameter == nullptr || entry.controllerId == 0)
                    continue;

                auto mappingState = te::createValueTree(te::IDs::MAP,
                                                        te::IDs::id, entry.controllerId,
                                                        te::IDs::channel, entry.channel,
                                                        te::IDs::param, entry.parameter->getFullName(),
                                                        te::IDs::pluginID, entry.parameter->getOwnerID());
                state.addChild(mappingState, -1, undoManager);
            }
        }

        mappings.loadFromEdit();
    }

    std::vector<Entry> entries;
};

AutomatableSliderComponent::AutomatableSliderComponent(const tracktion_engine::AutomatableParameter::Ptr ap)
    : m_automatableParameter(ap)
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, 0, 0, false);
    setWantsKeyboardFocus(true);

    if (m_automatableParameter != nullptr)
    {
        m_midiLearnListener = std::make_unique<MidiLearnListener>(*this, m_automatableParameter->getEdit().engine.getMidiLearnState());
        bindSliderToParameter();
        m_automatableParameter->addListener(this);

        if (auto t = m_automatableParameter->getTrack())
            m_trackColour = t->getColour();
    }
    else
    {
        setEnabled(false);
    }

    m_modDepthSlider.setSliderStyle(juce::Slider::LinearBar);
    m_modDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    m_modDepthSlider.setRange(-1.0, 1.0);
    m_modDepthSlider.setValue(1.0, juce::dontSendNotification);
    m_modDepthSlider.setVisible(false);

    // Theme colors
    auto thumbColour = juce::Colours::orange;
    auto trackColour = juce::Colours::darkgrey;
    auto bgColour = juce::Colours::black.withAlpha(0.5f);

    if (auto *lnf = dynamic_cast<NextLookAndFeel *>(&getLookAndFeel()))
    {
        thumbColour = lnf->getPrimeColour();
        trackColour = lnf->getBackgroundColour2();
        bgColour = lnf->getBackgroundColour1().withAlpha(0.5f);
    }

    m_modDepthSlider.setColour(juce::Slider::thumbColourId, thumbColour);
    m_modDepthSlider.setColour(juce::Slider::trackColourId, trackColour);
    m_modDepthSlider.setColour(juce::Slider::backgroundColourId, bgColour);

    m_modDepthSlider.setAlpha(0.7f);
    addChildComponent(m_modDepthSlider);
    m_modDepthSlider.addMouseListener(this, false);

    if (m_automatableParameter != nullptr)
        if (auto def = m_automatableParameter->getDefaultValue())
            setDoubleClickReturnValue(true, *def);

    m_modDepthSlider.onValueChange = [this]
    {
        if (m_automatableParameter == nullptr)
            return;

        auto assignments = m_automatableParameter->getAssignments();
        if (!assignments.isEmpty())
        {
            auto &ass = *assignments[0];
            ass.value = (float)m_modDepthSlider.getValue();
            repaint();
        }
    };

    updateModDepthVisibility();
    refreshMidiMappingState();
}

AutomatableSliderComponent::~AutomatableSliderComponent()
{
    cancelMidiLearn();
    m_modDepthSlider.removeMouseListener(this);
    m_midiLearnListener.reset();

    if (m_automatableParameter != nullptr)
        m_automatableParameter->removeListener(this);
}

void AutomatableSliderComponent::mouseEnter(const juce::MouseEvent &)
{
    refreshMidiMappingState();
    updateModDepthVisibility();
    repaint();
}

void AutomatableSliderComponent::mouseExit(const juce::MouseEvent &)
{
    updateModDepthVisibility();
    repaint();
}

void AutomatableSliderComponent::enablementChanged() { setAlpha(isEnabled() ? 1.0f : 0.5f); }

void AutomatableSliderComponent::paintOverChildren(juce::Graphics &g)
{
    juce::Slider::paintOverChildren(g);

    if (m_automatableParameter == nullptr)
        return;

    if (m_midiControllerId >= 0)
    {
        auto badgeArea = getLocalBounds().toFloat();
        badgeArea.reduce(0.f, 5.f);
        auto badgeBounds = badgeArea.removeFromTop(5.0f).removeFromRight(5.f);
        g.setColour(juce::Colours::deepskyblue.withAlpha(isMouseOver(true) ? 0.95f : 0.75f));
        g.fillEllipse(badgeBounds);
    }

    if (m_isMidiLearnPending)
    {
        const auto bounds = getLocalBounds().toFloat().reduced(5.0f);
        const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - m_midiLearnStartedMs;
        const auto pulse = 0.5f + 0.5f * std::sin((float)(elapsedMs * 0.012));
        const auto borderColour = juce::Colours::orange.withAlpha(0.5f + 0.35f * pulse);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 10.0f, 2.0f + pulse * 0.8f);

        auto textBounds = getLocalBounds().reduced(8, 10).removeFromBottom(18);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions{10.0f}));
        g.drawFittedText("Learn...", textBounds, juce::Justification::centred, 1);
    }
}

bool AutomatableSliderComponent::keyPressed(const juce::KeyPress &key)
{
    if (key == juce::KeyPress::escapeKey && m_isMidiLearnPending)
    {
        cancelMidiLearn();
        return true;
    }

    return juce::Slider::keyPressed(key);
}

void AutomatableSliderComponent::resized()
{
    juce::Slider::resized();
    m_modDepthSlider.setBounds(getLocalBounds().removeFromBottom(10));
}

void AutomatableSliderComponent::updateModDepthVisibility()
{
    if (m_automatableParameter == nullptr)
    {
        m_modDepthSlider.setVisible(false);
        return;
    }

    auto assignments = m_automatableParameter->getAssignments();

    // Check if mouse is over this component or the depth slider itself
    bool mouseIsOver = isMouseOver(true) || m_modDepthSlider.isMouseOverOrDragging();
    bool shouldBeVisible = !assignments.isEmpty() && mouseIsOver;

    if (shouldBeVisible)
    {
        auto &ass = *assignments[0];
        if (!m_modDepthSlider.isMouseButtonDown())
            m_modDepthSlider.setValue(ass.value.get(), juce::dontSendNotification);

        m_modDepthSlider.setVisible(true);
        m_modDepthSlider.toFront(false);
    }
    else
    {
        // Don't hide if we are currently dragging the depth slider
        if (!m_modDepthSlider.isMouseButtonDown())
            m_modDepthSlider.setVisible(false);
    }
}

void AutomatableSliderComponent::mouseDown(const juce::MouseEvent &e)
{
    if (m_automatableParameter == nullptr)
        return;

    if (e.mods.isRightButtonDown())
    {
        juce::PopupMenu m;

        auto assignments = m_automatableParameter->getAssignments();
        if (!assignments.isEmpty())
        {
            juce::PopupMenu modifierMenu;
            int itemId = removeModifierMenuBaseId;

            auto *track = m_automatableParameter->getTrack();
            if (auto *modifierList = track != nullptr ? track->getModifierList() : nullptr)
            {
                for (auto *modifier : modifierList->getModifiers())
                {
                    for (auto &assignment : assignments)
                    {
                        if (assignment->isForModifierSource(*modifier))
                        {
                            juce::String modifierName = modifier->getName();
                            if (modifierName.isEmpty())
                                modifierName = "Modifier";

                            modifierMenu.addItem(itemId++, modifierName);
                            break;
                        }
                    }
                }
            }

            m.addSubMenu("Remove Modifier", modifierMenu);
        }

        m.addItem(midiLearnMenuId, m_midiControllerId >= 0 ? "Change MIDI Mapping" : "MIDI Learn");
        m.addItem(removeMidiMappingMenuId, "Remove MIDI Mapping", m_midiControllerId >= 0);
        m.addSeparator();

        if (m_automatableParameter->getCurve().getNumPoints() == 0)
        {
            m.addItem(addAutomationLaneMenuId, "Add automation lane");
        }
        else
        {
            m.addItem(clearAutomationMenuId, "Clear automation");
        }

        const int result = m.show();

        if (result == midiLearnMenuId)
        {
            beginMidiLearn(m_midiControllerId >= 0);
        }
        else if (result == removeMidiMappingMenuId)
        {
            removeMidiMapping();
        }
        else if (result == addAutomationLaneMenuId)
        {
            auto start = tracktion::core::TimePosition::fromSeconds(0.0);
            m_automatableParameter->getCurve().addPoint(start, (float)getValue(), 0.0);

            if (auto *track = m_automatableParameter->getTrack())
                track->state.setProperty(IDs::isTrackMinimized, false, nullptr);
        }
        else if (result == clearAutomationMenuId)
        {
            m_automatableParameter->getCurve().clear();
        }
        else if (result >= removeModifierMenuBaseId && result < midiLearnMenuId)
        {
            int index = result - removeModifierMenuBaseId;
            if (index >= 0 && index < assignments.size())
            {
                m_automatableParameter->removeModifier(*assignments[index]);
            }
        }
    }
    else if (e.originalComponent != &m_modDepthSlider)
    {
        juce::Slider::mouseDown(e);
    }
}

void AutomatableSliderComponent::mouseDrag(const juce::MouseEvent &e)
{
    if (e.originalComponent != &m_modDepthSlider)
        juce::Slider::mouseDrag(e);
}

void AutomatableSliderComponent::mouseUp(const juce::MouseEvent &e)
{
    if (e.originalComponent != &m_modDepthSlider)
        juce::Slider::mouseUp(e);
}

void AutomatableSliderComponent::setTrackColour(juce::Colour colour) { m_trackColour = colour; }
juce::Colour AutomatableSliderComponent::getTrackColour() const { return m_trackColour; }

te::AutomatableParameter::Ptr AutomatableSliderComponent::getAutomatableParameter() { return m_automatableParameter; }

void AutomatableSliderComponent::setParameter(te::AutomatableParameter::Ptr newParam)
{
    if (m_automatableParameter == newParam)
        return;

    cancelMidiLearn();

    if (m_automatableParameter)
        m_automatableParameter->removeListener(this);

    m_midiLearnListener.reset();

    m_automatableParameter = newParam;

    if (m_automatableParameter)
    {
        m_midiLearnListener = std::make_unique<MidiLearnListener>(*this, m_automatableParameter->getEdit().engine.getMidiLearnState());
        m_automatableParameter->addListener(this);
        bindSliderToParameter();

        if (auto t = m_automatableParameter->getTrack())
            m_trackColour = t->getColour();

        // Reset double click value if available
        if (auto def = m_automatableParameter->getDefaultValue())
            setDoubleClickReturnValue(true, *def);
        else
            setDoubleClickReturnValue(false, 0.0);

        updateModDepthVisibility();
        refreshMidiMappingState();
    }
    else
    {
        setEnabled(false);
        m_midiChannel = -1;
        m_midiControllerId = -1;
        updateTooltip();
    }

    repaint();
}

void AutomatableSliderComponent::bindSliderToParameter()
{
    if (m_automatableParameter == nullptr)
    {
        setEnabled(false);
        return;
    }

    const auto v = m_automatableParameter->valueRange;
    const auto range = juce::NormalisableRange<double>(static_cast<double>(v.start), static_cast<double>(v.end), static_cast<double>(v.interval), static_cast<double>(v.skew), v.symmetricSkew);

    setNormalisableRange(range);
    setValue(m_automatableParameter->getCurrentBaseValue(), juce::dontSendNotification);
}

bool AutomatableSliderComponent::hasAnAutomatableParameter() { return m_automatableParameter != nullptr; }

void AutomatableSliderComponent::chooseAutomatableParameter(std::function<void(te::AutomatableParameter::Ptr)> handleChosenParam, std::function<void()> /*startLearnMode*/)
{
    if (handleChosenParam && m_automatableParameter)
        handleChosenParam(m_automatableParameter);
}

void AutomatableSliderComponent::curveHasChanged(te::AutomatableParameter &) {}

void AutomatableSliderComponent::currentValueChanged(te::AutomatableParameter &)
{
    if (m_automatableParameter)
    {
        double newVal = m_automatableParameter->getCurrentBaseValue();

        if (!isMouseButtonDown() && std::abs(getValue() - newVal) > 0.0001)
            setValue(newVal, juce::dontSendNotification);

        repaint();
    }
}

void AutomatableSliderComponent::startedDragging()
{
    if (m_automatableParameter)
        m_automatableParameter->beginParameterChangeGesture();
}

void AutomatableSliderComponent::stoppedDragging()
{
    if (m_automatableParameter)
        m_automatableParameter->endParameterChangeGesture();
}

void AutomatableSliderComponent::valueChanged()
{
    if (m_automatableParameter)
    {
        float val = static_cast<float>(getValue());
        m_automatableParameter->setParameter(val, juce::sendNotification);
    }
}

void AutomatableSliderComponent::beginMidiLearn(bool replaceExistingMapping)
{
    if (m_automatableParameter == nullptr)
        return;

    auto *learnState = getMidiLearnState();

    if (learnState == nullptr || getParameterControlMappings() == nullptr)
        return;

    juce::ignoreUnused(replaceExistingMapping);

    m_midiMappingSnapshot = std::make_unique<MidiMappingSnapshot>();
    m_midiMappingSnapshot->capture(*getParameterControlMappings());

    refreshMidiMappingState();
    m_isMidiLearnPending = true;
    m_midiLearnStartedMs = juce::Time::getMillisecondCounterHiRes();
    updateTooltip();

    learnState->setActive(true);
    m_automatableParameter->getEdit().getParameterChangeHandler().parameterChanged(*m_automatableParameter, false);

    startTimerHz(30);
    grabKeyboardFocus();
    repaint();
}

void AutomatableSliderComponent::cancelMidiLearn()
{
    if (m_automatableParameter != nullptr)
    {
        auto &changeHandler = m_automatableParameter->getEdit().getParameterChangeHandler();

        if (changeHandler.getPendingParam(false).get() == m_automatableParameter.get())
            changeHandler.getPendingParam(true);
    }

    if (m_isMidiLearnPending)
        if (auto *learnState = getMidiLearnState())
            if (learnState->isActive())
                learnState->setActive(false);

    finishMidiLearn();
}

void AutomatableSliderComponent::finishMidiLearn()
{
    m_isMidiLearnPending = false;
    stopTimer();
    refreshMidiMappingState();
    m_midiMappingSnapshot.reset();
    repaint();
}

bool AutomatableSliderComponent::removeMidiMapping()
{
    auto *mappings = getParameterControlMappings();

    if (mappings == nullptr)
        return false;

    bool removedAny = false;

    while (mappings->removeParameterMapping(*m_automatableParameter))
        removedAny = true;

    if (removedAny)
    {
        refreshMidiMappingState();

        if (auto *learnState = getMidiLearnState())
            learnState->assignmentChanged(te::MidiLearnState::removed);
    }

    return removedAny;
}

void AutomatableSliderComponent::restoreMidiMappingSnapshot()
{
    auto *mappings = getParameterControlMappings();

    if (mappings == nullptr || m_automatableParameter == nullptr || m_midiMappingSnapshot == nullptr)
        return;

    m_midiMappingSnapshot->restore(m_automatableParameter->getEdit(), *mappings);
    refreshMidiMappingState();

    if (auto *learnState = getMidiLearnState())
        juce::MessageManager::callAsync([learnState]
        {
            learnState->assignmentChanged(te::MidiLearnState::removed);
        });
}

bool AutomatableSliderComponent::resolveMidiMappingConflict()
{
    auto *mappings = getParameterControlMappings();

    if (mappings == nullptr || m_automatableParameter == nullptr || m_midiControllerId < 0)
        return true;

    juce::Array<int> conflictingRows;
    juce::StringArray conflictingParameters;

    for (int row = mappings->getNumControllerIDs(); --row >= 0;)
    {
        auto mapping = mappings->getMappingForRow(row);

        if (mapping.parameter != nullptr
            && mapping.parameter != m_automatableParameter.get()
            && mapping.controllerID == m_midiControllerId
            && mapping.channelID == m_midiChannel)
        {
            conflictingRows.add(row);
            conflictingParameters.add(mapping.parameter->getFullName());
        }
    }

    if (conflictingRows.isEmpty())
        return true;

    const auto message = "This MIDI controller is already mapped to:\n\n"
                         + conflictingParameters.joinIntoString("\n")
                         + "\n\nWhat would you like to do?";

    const int result = juce::AlertWindow::showYesNoCancelBox(juce::AlertWindow::QuestionIcon,
                                                            "MIDI Controller Already Mapped",
                                                            message,
                                                            "Replace Old",
                                                            "Add New",
                                                            "Cancel");

    switch (result)
    {
    case 1: // Replace Old
        for (auto row : conflictingRows)
            mappings->removeMapping(row);

        if (auto *learnState = getMidiLearnState())
            juce::MessageManager::callAsync([learnState]
            {
                learnState->assignmentChanged(te::MidiLearnState::removed);
            });

        refreshMidiMappingState();
        return true;

    case 2: // Add New
        refreshMidiMappingState();
        return true;

    case 3: // Cancel
    default:
        restoreMidiMappingSnapshot();
        return false;
    }
}

void AutomatableSliderComponent::refreshMidiMappingState()
{
    m_midiChannel = -1;
    m_midiControllerId = -1;

    if (auto *mappings = getParameterControlMappings())
        mappings->getParameterMapping(*m_automatableParameter, m_midiChannel, m_midiControllerId);

    updateTooltip();
}

void AutomatableSliderComponent::updateTooltip()
{
    if (m_midiControllerId >= 0)
        setTooltip("MIDI mapped: " + getMidiMappingLabel());
    else if (m_isMidiLearnPending)
        setTooltip("Waiting for MIDI controller input. Press Esc to cancel.");
    else
        setTooltip(juce::String{});
}

void AutomatableSliderComponent::timerCallback()
{
    if (!m_isMidiLearnPending)
    {
        stopTimer();
        return;
    }

    if ((juce::Time::getMillisecondCounterHiRes() - m_midiLearnStartedMs) >= midiLearnTimeoutMs)
    {
        cancelMidiLearn();
        return;
    }

    repaint();
}

void AutomatableSliderComponent::handleMidiLearnStatusChanged(bool isActive)
{
    if (!isActive && m_isMidiLearnPending)
        finishMidiLearn();

    updateTooltip();
    repaint();
}

void AutomatableSliderComponent::handleMidiLearnAssignmentChanged(te::MidiLearnState::ChangeType)
{
    refreshMidiMappingState();

    if (m_isMidiLearnPending && m_midiControllerId >= 0)
    {
        resolveMidiMappingConflict();

        if (auto *learnState = getMidiLearnState())
            if (learnState->isActive())
                learnState->setActive(false);

        finishMidiLearn();
        return;
    }

    repaint();
}

te::MidiLearnState *AutomatableSliderComponent::getMidiLearnState() const
{
    if (m_automatableParameter == nullptr)
        return nullptr;

    return &m_automatableParameter->getEdit().engine.getMidiLearnState();
}

te::ParameterControlMappings *AutomatableSliderComponent::getParameterControlMappings() const
{
    if (m_automatableParameter == nullptr)
        return nullptr;

    return &m_automatableParameter->getEdit().getParameterControlMappings();
}

juce::String AutomatableSliderComponent::getMidiMappingLabel() const
{
    if (m_midiControllerId < 0)
        return {};

    return getControllerDescription(m_midiControllerId, m_midiChannel);
}
