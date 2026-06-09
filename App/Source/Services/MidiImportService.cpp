/*

This file is part of NextStudio.
Copyright (c) Steffen Baranowsky 2019-2025.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see https://www.gnu.org/licenses/.

==============================================================================
*/

#include "Services/MidiImportService.h"
#include "Utilities/Utilities.h"

namespace
{
struct ImportedNote
{
    int noteNumber = 60;
    double startBeat = 0.0;
    double lengthBeats = 1.0;
    float velocity = 0.8f;
    int channel = 1;
};

struct ImportedTrack
{
    juce::String name;
    juce::Array<ImportedNote> notes;
    double endBeat = 0.0;
};

double getMidiPpq(const juce::MidiFile& midiFile)
{
    const auto timeFormat = midiFile.getTimeFormat();

    // Positive time format means ticks per quarter note.
    if (timeFormat > 0)
        return static_cast<double>(timeFormat);

    return 0.0;
}

juce::String getTrackName(const juce::MidiMessageSequence& sequence, int fallbackIndex)
{
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto* holder = sequence.getEventPointer(i);
        if (holder == nullptr)
            continue;

        const auto& message = holder->message;
        if (message.isTrackNameEvent())
            return message.getTextFromTextMetaEvent().trim();
    }

    return "MIDI Track " + juce::String(fallbackIndex + 1);
}

ImportedTrack parseTrack(const juce::MidiMessageSequence& sourceSequence, int trackIndex, double ppq)
{
    ImportedTrack importedTrack;
    importedTrack.name = getTrackName(sourceSequence, trackIndex);

    juce::MidiMessageSequence sequence(sourceSequence);
    sequence.updateMatchedPairs();

    for (int eventIndex = 0; eventIndex < sequence.getNumEvents(); ++eventIndex)
    {
        const auto* holder = sequence.getEventPointer(eventIndex);
        if (holder == nullptr)
            continue;

        const auto& message = holder->message;
        if (!message.isNoteOn())
            continue;

        const auto* noteOffHolder = holder->noteOffObject;
        if (noteOffHolder == nullptr)
            continue;

        const double startBeat = message.getTimeStamp() / ppq;
        const double endBeat = noteOffHolder->message.getTimeStamp() / ppq;
        const double lengthBeats = juce::jmax(1.0 / 64.0, endBeat - startBeat);

        ImportedNote importedNote;
        importedNote.noteNumber = message.getNoteNumber();
        importedNote.startBeat = startBeat;
        importedNote.lengthBeats = lengthBeats;
        importedNote.velocity = juce::jlimit(0.01f, 1.0f, message.getFloatVelocity());
        importedNote.channel = message.getChannel();

        importedTrack.notes.add(importedNote);
        importedTrack.endBeat = juce::jmax(importedTrack.endBeat, startBeat + lengthBeats);
    }

    return importedTrack;
}

tracktion::TimeDuration beatDurationToTime(EditViewState& editViewState, double beats)
{
    return tracktion::TimeDuration::fromSeconds(juce::jmax(0.001, editViewState.beatToTime(beats)));
}

void importTrackNotes(EditViewState& editViewState, const ImportedTrack& importedTrack, juce::Colour trackColour)
{
    if (importedTrack.notes.isEmpty())
        return;

    auto track = EngineHelpers::addAudioTrack(true, trackColour, editViewState);
    if (track == nullptr)
        return;

    track->setName(importedTrack.name);

    const auto clipLength = beatDurationToTime(editViewState, juce::jmax(1.0, importedTrack.endBeat));
    tracktion::TimeRange clipRange(tracktion::TimePosition::fromSeconds(0.0), clipLength);

    auto midiClip = track->insertMIDIClip(clipRange, &editViewState.m_selectionManager);
    if (midiClip == nullptr)
        return;

    midiClip->setName(importedTrack.name);

    auto& sequence = midiClip->getSequence();
    auto& undoManager = editViewState.m_edit.getUndoManager();

    for (const auto& note : importedTrack.notes)
    {
        sequence.addNote(note.noteNumber,
                         tracktion::core::BeatPosition::fromBeats(note.startBeat),
                         tracktion::core::BeatDuration::fromBeats(note.lengthBeats),
                         note.velocity,
                         111,
                         &undoManager);
    }
}
}

MidiImportReport MidiImportService::importMidiFile(EditViewState& editViewState, const juce::File& midiFile)
{
    MidiImportReport report;

    if (!midiFile.existsAsFile())
    {
        report.message = "MIDI file does not exist.";
        return report;
    }

    juce::FileInputStream inputStream(midiFile);
    if (!inputStream.openedOk())
    {
        report.message = "Could not open MIDI file.";
        return report;
    }

    juce::MidiFile parsedMidiFile;
    if (!parsedMidiFile.readFrom(inputStream))
    {
        report.message = "Could not parse MIDI file.";
        return report;
    }

    const double ppq = getMidiPpq(parsedMidiFile);
    if (ppq <= 0.0)
    {
        report.message = "SMPTE MIDI files are not supported yet. Please use PPQ-based MIDI files.";
        return report;
    }

    juce::Array<ImportedTrack> tracksToImport;
    for (int trackIndex = 0; trackIndex < parsedMidiFile.getNumTracks(); ++trackIndex)
    {
        const auto* sequence = parsedMidiFile.getTrack(trackIndex);
        if (sequence == nullptr)
            continue;

        auto importedTrack = parseTrack(*sequence, trackIndex, ppq);
        if (!importedTrack.notes.isEmpty())
            tracksToImport.add(importedTrack);
    }

    if (tracksToImport.isEmpty())
    {
        report.message = "The selected MIDI file has no note data.";
        return report;
    }

    auto& undoManager = editViewState.m_edit.getUndoManager();
    undoManager.beginNewTransaction("Import MIDI file");

    for (const auto& importedTrack : tracksToImport)
    {
        importTrackNotes(editViewState, importedTrack, editViewState.m_applicationState.getRandomTrackColour());
        report.importedTracks++;
        report.importedNotes += importedTrack.notes.size();
    }

    editViewState.m_trackHeightManager->regenerateTrackHeightsFromEdit(editViewState.m_edit);
    editViewState.m_edit.restartPlayback();
    editViewState.m_edit.sendSourceFileUpdate();

    report.message = "Imported " + juce::String(report.importedTracks) + " MIDI track(s), " + juce::String(report.importedNotes) + " note(s).";
    return report;
}
