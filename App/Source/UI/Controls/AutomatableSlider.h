/*
  ==============================================================================

    AutomatableSlider.h
    Created: 15 Jan 2026
    Author:  NextStudio

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Utilities/EditViewState.h"
#include "Utilities/Utilities.h"

namespace te = tracktion_engine;

class AutomatableSliderComponent
    : public juce::Slider
    , public te::AutomationDragDropTarget
    , public te::AutomatableParameter::Listener
    , private juce::Timer
{
public:
    explicit AutomatableSliderComponent(const te::AutomatableParameter::Ptr ap);
    ~AutomatableSliderComponent() override;

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;

    te::AutomatableParameter::Ptr getAutomatableParameter();
    void setParameter(te::AutomatableParameter::Ptr newParam);
    void bindSliderToParameter();

    [[nodiscard]] juce::Colour getTrackColour() const;
    void setTrackColour(juce::Colour colour);

    // AutomationDragDropTarget overrides
    bool hasAnAutomatableParameter() override;
    void chooseAutomatableParameter(std::function<void(te::AutomatableParameter::Ptr)> handleChosenParam, std::function<void()> startLearnMode) override;

    void curveHasChanged(te::AutomatableParameter &) override;
    void currentValueChanged(te::AutomatableParameter &) override;

    void startedDragging() override;
    void stoppedDragging() override;
    void valueChanged() override;

    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void enablementChanged() override;
    void paintOverChildren(juce::Graphics &g) override;
    bool keyPressed(const juce::KeyPress &key) override;
    void resized() override;

private:
    struct MidiLearnListener;
    struct MidiMappingSnapshot;
    friend struct MidiLearnListener;

    void updateModDepthVisibility();
    void beginMidiLearn(bool replaceExistingMapping);
    void cancelMidiLearn();
    void finishMidiLearn();
    bool removeMidiMapping();
    void restoreMidiMappingSnapshot();
    bool resolveMidiMappingConflict();
    void refreshMidiMappingState();
    void updateTooltip();
    void timerCallback() override;
    void handleMidiLearnStatusChanged(bool isActive);
    void handleMidiLearnAssignmentChanged(te::MidiLearnState::ChangeType);
    [[nodiscard]] te::MidiLearnState *getMidiLearnState() const;
    [[nodiscard]] te::ParameterControlMappings *getParameterControlMappings() const;
    [[nodiscard]] juce::String getMidiMappingLabel() const;

    juce::Slider m_modDepthSlider;
    te::AutomatableParameter::Ptr m_automatableParameter;
    std::unique_ptr<MidiLearnListener> m_midiLearnListener;
    std::unique_ptr<MidiMappingSnapshot> m_midiMappingSnapshot;
    juce::Colour m_trackColour;
    bool m_isMidiLearnPending = false;
    int m_midiChannel = -1;
    int m_midiControllerId = -1;
    double m_midiLearnStartedMs = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomatableSliderComponent)
};
