/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "notationactioncontroller.h"

#include <QPoint>

#include "log.h"
#include "notationtypes.h"

using namespace mu::notation;
using namespace mu::actions;
using namespace mu::context;

static constexpr int INVALID_BOX_INDEX = -1;
static constexpr qreal STRETCH_STEP = 0.1;
static const ActionCode ESCAPE_ACTION_CODE = "escape";
static const ActionCode UNDO_ACTION_CODE = "undo";
static const ActionCode REDO_ACTION_CODE = "redo";

void NotationActionController::init()
{
    //! NOTE For historical reasons, the name of the action does not match what needs to be done
    registerAction(ESCAPE_ACTION_CODE, &NotationActionController::resetState, &NotationActionController::isNotationPage);

    registerAction("note-input", [this]() { toggleNoteInput(); });
    registerNoteInputAction("note-input-steptime", NoteInputMethod::STEPTIME);
    registerNoteInputAction("note-input-rhythm", NoteInputMethod::RHYTHM);
    registerNoteInputAction("note-input-repitch", NoteInputMethod::REPITCH);
    registerNoteInputAction("note-input-realtime-auto", NoteInputMethod::REALTIME_AUTO);
    registerNoteInputAction("note-input-realtime-manual", NoteInputMethod::REALTIME_MANUAL);
    registerNoteInputAction("note-input-timewise", NoteInputMethod::TIMEWISE);

    registerPadNoteAction("note-longa", Pad::NOTE00);
    registerPadNoteAction("note-breve", Pad::NOTE0);
    registerPadNoteAction("pad-note-1", Pad::NOTE1);
    registerPadNoteAction("pad-note-2", Pad::NOTE2);
    registerPadNoteAction("pad-note-4", Pad::NOTE4);
    registerPadNoteAction("pad-note-8", Pad::NOTE8);
    registerPadNoteAction("pad-note-16", Pad::NOTE16);
    registerPadNoteAction("pad-note-32", Pad::NOTE32);
    registerPadNoteAction("pad-note-64", Pad::NOTE64);
    registerPadNoteAction("pad-note-128", Pad::NOTE128);
    registerPadNoteAction("pad-note-256", Pad::NOTE256);
    registerPadNoteAction("pad-note-512", Pad::NOTE512);
    registerPadNoteAction("pad-note-1024", Pad::NOTE1024);
    registerPadNoteAction("pad-dot", Pad::DOT);
    registerPadNoteAction("pad-dotdot", Pad::DOTDOT);
    registerPadNoteAction("pad-dot3", Pad::DOT3);
    registerPadNoteAction("pad-dot4", Pad::DOT4);
    registerPadNoteAction("pad-rest", Pad::REST);

    registerNoteAction("note-c", NoteName::C);
    registerNoteAction("note-d", NoteName::D);
    registerNoteAction("note-e", NoteName::E);
    registerNoteAction("note-f", NoteName::F);
    registerNoteAction("note-g", NoteName::G);
    registerNoteAction("note-a", NoteName::A);
    registerNoteAction("note-b", NoteName::B);

    registerNoteAction("chord-c", NoteName::C, NoteAddingMode::CurrentChord);
    registerNoteAction("chord-d", NoteName::D, NoteAddingMode::CurrentChord);
    registerNoteAction("chord-e", NoteName::E, NoteAddingMode::CurrentChord);
    registerNoteAction("chord-f", NoteName::F, NoteAddingMode::CurrentChord);
    registerNoteAction("chord-g", NoteName::G, NoteAddingMode::CurrentChord);
    registerNoteAction("chord-a", NoteName::A, NoteAddingMode::CurrentChord);
    registerNoteAction("chord-b", NoteName::B, NoteAddingMode::CurrentChord);

    registerNoteAction("insert-c", NoteName::C, NoteAddingMode::InsertChord);
    registerNoteAction("insert-d", NoteName::D, NoteAddingMode::InsertChord);
    registerNoteAction("insert-e", NoteName::E, NoteAddingMode::InsertChord);
    registerNoteAction("insert-f", NoteName::F, NoteAddingMode::InsertChord);
    registerNoteAction("insert-g", NoteName::G, NoteAddingMode::InsertChord);
    registerNoteAction("insert-a", NoteName::A, NoteAddingMode::InsertChord);
    registerNoteAction("insert-b", NoteName::B, NoteAddingMode::InsertChord);

    registerLyricsAction("next-lyric", &NotationActionController::nextLyrics);
    registerLyricsAction("prev-lyric", &NotationActionController::previousLyrics);
    registerLyricsAction("next-lyric-verse", &NotationActionController::nextLyricsVerse);
    registerLyricsAction("prev-lyric-verse", &NotationActionController::previousLyricsVerse);
    registerLyricsAction("next-syllable", &NotationActionController::nextSyllable);
    registerLyricsAction("add-melisma", &NotationActionController::addMelisma);
    registerLyricsAction("add-lyric-verse", &NotationActionController::addLyricsVerse);

    registerAction("flat2", [this]() { toggleAccidental(AccidentalType::FLAT2); });
    registerAction("flat", [this]() { toggleAccidental(AccidentalType::FLAT); });
    registerAction("nat", [this]() { toggleAccidental(AccidentalType::NATURAL); });
    registerAction("sharp", [this]() { toggleAccidental(AccidentalType::SHARP); });
    registerAction("sharp2", [this]() { toggleAccidental(AccidentalType::SHARP2); });

    registerAction("rest", &NotationActionController::putRestToSelection);
    registerAction("rest-1", [this]() { putRest(DurationType::V_WHOLE); });
    registerAction("rest-2", [this]() { putRest(DurationType::V_HALF); });
    registerAction("rest-4", [this]() { putRest(DurationType::V_QUARTER); });
    registerAction("rest-8", [this]() { putRest(DurationType::V_EIGHTH); });

    registerAction("add-marcato", [this]() { addArticulation(SymbolId::articMarcatoAbove); });
    registerAction("add-sforzato", [this]() { addArticulation(SymbolId::articAccentAbove); });
    registerAction("add-tenuto", [this]() { addArticulation(SymbolId::articTenutoAbove); });
    registerAction("add-staccato", [this]() { addArticulation(SymbolId::articStaccatoAbove); });

    registerAction("duplet", [this]() { putTuplet(2); });
    registerAction("triplet", [this]() { putTuplet(3); });
    registerAction("quadruplet", [this]() { putTuplet(4); });
    registerAction("quintuplet", [this]() { putTuplet(5); });
    registerAction("sextuplet", [this]() { putTuplet(6); });
    registerAction("septuplet", [this]() { putTuplet(7); });
    registerAction("octuplet", [this]() { putTuplet(8); });
    registerAction("nonuplet", [this]() { putTuplet(9); });
    registerAction("tuplet-dialog", &NotationActionController::openTupletOtherDialog);

    registerAction("put-note", &NotationActionController::putNote);

    registerAction("toggle-visible", &NotationActionController::toggleVisible);

    registerMoveAction("next-element");
    registerMoveAction("prev-element");
    registerMoveAction("next-chord");
    registerMoveAction("prev-chord");
    registerMoveAction("next-measure");
    registerMoveAction("prev-measure");
    registerMoveAction("next-track");
    registerMoveAction("prev-track");
    registerMoveAction("pitch-up");
    registerMoveAction("pitch-down");
    registerMoveAction("pitch-up-octave");
    registerMoveAction("pitch-down-octave");
    registerAction("move-up", [this]() { moveChordRestToStaff(MoveDirection::Up); }, &NotationActionController::hasSelection);
    registerAction("move-down", [this]() { moveChordRestToStaff(MoveDirection::Down); }, &NotationActionController::hasSelection);

    registerAction("double-duration", [this]() { increaseDecreaseDuration(-1, /*stepByDots*/ false); });
    registerAction("half-duration", [this]() { increaseDecreaseDuration(1, false); });
    registerAction("inc-duration-dotted", [this]() { increaseDecreaseDuration(-1, true); });
    registerAction("dec-duration-dotted", [this]() { increaseDecreaseDuration(1, true); });

    registerAction("cut", &NotationActionController::cutSelection, &NotationActionController::hasSelection);
    registerAction("copy", &NotationActionController::copySelection, &NotationActionController::hasSelection);
    registerAction("paste", [this]() { pasteSelection(PastingType::Default); }, &NotationActionController::isNotationPage);
    registerAction("paste-half", [this]() { pasteSelection(PastingType::Half); });
    registerAction("paste-double", [this]() { pasteSelection(PastingType::Double); });
    registerAction("paste-special", [this]() { pasteSelection(PastingType::Special); });
    registerAction("swap", &NotationActionController::swapSelection);
    registerAction("delete", &NotationActionController::deleteSelection, &NotationActionController::hasSelection);
    registerAction("flip", &NotationActionController::flipSelection);
    registerAction("tie", &NotationActionController::addTie);
    registerAction("chord-tie", &NotationActionController::chordTie);
    registerAction("add-slur", &NotationActionController::addSlur);

    registerAction(UNDO_ACTION_CODE, &NotationActionController::undo, &NotationActionController::canUndo);
    registerAction(REDO_ACTION_CODE, &NotationActionController::redo, &NotationActionController::canRedo);

    registerAction("select-next-chord", [this]() { addChordToSelection(MoveDirection::Right); });
    registerAction("select-prev-chord", [this]() { addChordToSelection(MoveDirection::Left); });
    registerAction("select-similar", &NotationActionController::selectAllSimilarElements);
    registerAction("select-similar-staff", &NotationActionController::selectAllSimilarElementsInStaff);
    registerAction("select-similar-range", &NotationActionController::selectAllSimilarElementsInRange);
    registerAction("select-dialog", &NotationActionController::openSelectionMoreOptions);
    registerAction("select-all", &NotationActionController::selectAll);
    registerAction("select-section", &NotationActionController::selectSection);
    registerAction("first-element", &NotationActionController::firstElement);
    registerAction("last-element", &NotationActionController::lastElement);
    registerAction("up-chord", [this]() { moveWithinChord(MoveDirection::Up); }, &NotationActionController::hasSelection);
    registerAction("down-chord", [this]() { moveWithinChord(MoveDirection::Down); }, &NotationActionController::hasSelection);
    registerAction("top-chord", [this]() { selectTopOrBottomOfChord(MoveDirection::Up); }, &NotationActionController::hasSelection);
    registerAction("bottom-chord", [this]() { selectTopOrBottomOfChord(MoveDirection::Down); }, &NotationActionController::hasSelection);

    registerAction("system-break", [this]() { toggleLayoutBreak(LayoutBreakType::LINE); });
    registerAction("page-break", [this]() { toggleLayoutBreak(LayoutBreakType::PAGE); });
    registerAction("section-break", [this]() { toggleLayoutBreak(LayoutBreakType::SECTION); });

    registerAction("split-measure", &NotationActionController::splitMeasure);
    registerAction("join-measures", &NotationActionController::joinSelectedMeasures);
    registerAction("insert-measures", &NotationActionController::selectMeasuresCountAndInsert);
    registerAction("append-measures", &NotationActionController::selectMeasuresCountAndAppend);
    registerAction("insert-measure", [this]() { insertBox(BoxType::Measure); });
    registerAction("append-measure", [this]() { appendBox(BoxType::Measure); });
    registerAction("insert-hbox", [this]() { insertBox(BoxType::Horizontal); });
    registerAction("insert-vbox", [this]() { insertBox(BoxType::Vertical); });
    registerAction("insert-textframe", [this]() { insertBox(BoxType::Text); });
    registerAction("append-hbox", [this]() { appendBox(BoxType::Horizontal); });
    registerAction("append-vbox", [this]() { appendBox(BoxType::Vertical); });
    registerAction("append-textframe", [this]() { appendBox(BoxType::Text); });

    registerAction("edit-style", &NotationActionController::openEditStyleDialog);
    registerAction("page-settings", &NotationActionController::openPageSettingsDialog);
    registerAction("staff-properties", &NotationActionController::openStaffProperties);
    registerAction("add-remove-breaks", &NotationActionController::openBreaksDialog);
    registerAction("edit-info", &NotationActionController::openScoreProperties);
    registerAction("transpose", &NotationActionController::openTransposeDialog);
    registerAction("parts", &NotationActionController::openPartsDialog);
    registerAction("staff-text-properties", &NotationActionController::openStaffTextPropertiesDialog);
    registerAction("system-text-properties", &NotationActionController::openStaffTextPropertiesDialog);
    registerAction("measure-properties", &NotationActionController::openMeasurePropertiesDialog);

    registerAction("voice-x12", [this]() { swapVoices(0, 1); });
    registerAction("voice-x13", [this]() { swapVoices(0, 2); });
    registerAction("voice-x14", [this]() { swapVoices(0, 3); });
    registerAction("voice-x23", [this]() { swapVoices(1, 2); });
    registerAction("voice-x24", [this]() { swapVoices(1, 3); });
    registerAction("voice-x34", [this]() { swapVoices(2, 3); });

    registerAction("add-8va", [this]() { addOttava(OttavaType::OTTAVA_8VA); });
    registerAction("add-8vb", [this]() { addOttava(OttavaType::OTTAVA_8VB); });
    registerAction("add-hairpin", [this]() { addHairpin(HairpinType::CRESC_HAIRPIN); });
    registerAction("add-hairpin-reverse", [this]() { addHairpin(HairpinType::DECRESC_HAIRPIN); });
    registerAction("add-noteline", &NotationActionController::addAnchoredNoteLine);

    registerAction("title-text", [this]() { addText(TextType::TITLE); });
    registerAction("subtitle-text", [this]() { addText(TextType::SUBTITLE); });
    registerAction("composer-text", [this]() { addText(TextType::COMPOSER); });
    registerAction("poet-text", [this]() { addText(TextType::POET); });
    registerAction("part-text", [this]() { addText(TextType::INSTRUMENT_EXCERPT); });
    registerAction("system-text", [this]() { addText(TextType::SYSTEM); });
    registerAction("staff-text", [this]() { addText(TextType::STAFF); });
    registerAction("expression-text", [this]() { addText(TextType::EXPRESSION); });
    registerAction("rehearsalmark-text", [this]() { addText(TextType::REHEARSAL_MARK); });
    registerAction("instrument-change-text", [this]() { addText(TextType::INSTRUMENT_CHANGE); });
    registerAction("fingering-text", [this]() { addText(TextType::FINGERING); });
    registerAction("sticking-text", [this]() { addText(TextType::STICKING); });
    registerAction("chord-text", [this]() { addText(TextType::HARMONY_A); });
    registerAction("roman-numeral-text", [this]() { addText(TextType::HARMONY_ROMAN); });
    registerAction("nashville-number-text", [this]() { addText(TextType::HARMONY_NASHVILLE); });
    registerAction("lyrics", [this]() { addText(TextType::LYRICS_ODD); });
    registerAction("figured-bass", [this]() { addFiguredBass(); });
    registerAction("tempo", [this]() { addText(TextType::TEMPO); });

    registerAction("stretch-", [this]() { addStretch(-STRETCH_STEP); });
    registerAction("stretch+", [this]() { addStretch(STRETCH_STEP); });

    registerAction("reset-stretch", &NotationActionController::resetStretch);
    registerAction("reset-text-style-overrides", &NotationActionController::resetTextStyleOverrides);
    registerAction("reset-beammode", &NotationActionController::resetBeamMode);
    registerAction("reset", &NotationActionController::resetShapesAndPosition);

    registerAction("show-invisible", [this]() { toggleScoreConfig(ScoreConfigType::ShowInvisibleElements); });
    registerAction("show-unprintable", [this]() { toggleScoreConfig(ScoreConfigType::ShowUnprintableElements); });
    registerAction("show-frames", [this]() { toggleScoreConfig(ScoreConfigType::ShowFrames); });
    registerAction("show-pageborders", [this]() { toggleScoreConfig(ScoreConfigType::ShowPageMargins); });
    registerAction("show-irregular", [this]() { toggleScoreConfig(ScoreConfigType::MarkIrregularMeasures); });

    registerAction("concert-pitch", &NotationActionController::toggleConcertPitch);

    registerAction("explode", &NotationActionController::explodeSelectedStaff);
    registerAction("implode", &NotationActionController::implodeSelectedStaff);
    registerAction("realize-chord-symbols", &NotationActionController::realizeSelectedChordSymbols);
    registerAction("time-delete", &NotationActionController::removeSelectedRange);
    registerAction("del-empty-measures", &NotationActionController::removeEmptyTrailingMeasures);
    registerAction("slash-fill", &NotationActionController::fillSelectionWithSlashes);
    registerAction("slash-rhythm", &NotationActionController::replaceSelectedNotesWithSlashes);
    registerAction("pitch-spell", &NotationActionController::spellPitches);
    registerAction("reset-groupings", &NotationActionController::regroupNotesAndRests);
    registerAction("resequence-rehearsal-marks", &NotationActionController::resequenceRehearsalMarks);
    registerAction("unroll-repeats", &NotationActionController::unrollRepeats);
    registerAction("copy-lyrics-to-clipboard", &NotationActionController::copyLyrics);

    registerAction("acciaccatura", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::ACCIACCATURA); });
    registerAction("appoggiatura", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::APPOGGIATURA); });
    registerAction("grace4", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::GRACE4); });
    registerAction("grace16", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::GRACE16); });
    registerAction("grace32", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::GRACE32); });
    registerAction("grace8after", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::GRACE8_AFTER); });
    registerAction("grace16after", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::GRACE16_AFTER); });
    registerAction("grace32after", [this]() { addGraceNotesToSelectedNotes(GraceNoteType::GRACE32_AFTER); });

    registerAction("beam-start", [this]() { addBeamToSelectedChordRests(BeamMode::BEGIN); });
    registerAction("beam-mid", [this]() { addBeamToSelectedChordRests(BeamMode::MID); });
    registerAction("no-beam", [this]() { addBeamToSelectedChordRests(BeamMode::NONE); });
    registerAction("beam-32", [this]() { addBeamToSelectedChordRests(BeamMode::BEGIN32); });
    registerAction("beam-64", [this]() { addBeamToSelectedChordRests(BeamMode::BEGIN64); });
    registerAction("auto-beam", [this]() { addBeamToSelectedChordRests(BeamMode::AUTO); });

    registerAction("add-brackets", [this]() { addBracketsToSelection(BracketsType::Brackets); });
    registerAction("add-parentheses", [this]() { addBracketsToSelection(BracketsType::Parentheses); });
    registerAction("add-braces", [this]() { addBracketsToSelection(BracketsType::Braces); });

    registerAction("enh-both", &NotationActionController::changeEnharmonicSpellingBoth);
    registerAction("enh-current", &NotationActionController::changeEnharmonicSpellingCurrent);

    registerTextAction("text-b", &NotationActionController::toggleBold);
    registerTextAction("text-i", &NotationActionController::toggleItalic);
    registerTextAction("text-u", &NotationActionController::toggleUnderline);

    for (int i = MIN_NOTES_INTERVAL; i <= MAX_NOTES_INTERVAL; ++i) {
        if (isNotesIntervalValid(i)) {
            registerAction("interval" + std::to_string(i), [this, i]() { addInterval(i); });
        }
    }

    for (int i = 0; i < Ms::VOICES; ++i) {
        registerAction("voice-" + std::to_string(i + 1), [this, i]() { changeVoice(i); });
    }

    for (int i = 0; i < MAX_FRET; ++i) {
        registerAction("fret-" + std::to_string(i), [this, i]() { addFret(i); }, &NotationActionController::isTablatureStaff);
    }

    // listen on state changes
    globalContext()->currentNotationChanged().onNotify(this, [this]() {
        auto notation = globalContext()->currentNotation();
        if (notation) {
            notation->interaction()->noteInput()->stateChanged().onNotify(this, [this]() {
                m_currentNotationNoteInputChanged.notify();
            });
        }
        m_currentNotationNoteInputChanged.notify();
    });
}

bool NotationActionController::canReceiveAction(const actions::ActionCode& code) const
{
    //! NOTE If the notation is not loaded, we cannot process anything.
    if (!currentNotation()) {
        return false;
    }

    auto iter = m_isEnabledMap.find(code);
    if (iter != m_isEnabledMap.end()) {
        return (this->*iter->second)();
    }

    return true;
}

INotationPtr NotationActionController::currentNotation() const
{
    return globalContext()->currentNotation();
}

INotationInteractionPtr NotationActionController::currentNotationInteraction() const
{
    return currentNotation() ? currentNotation()->interaction() : nullptr;
}

INotationSelectionPtr NotationActionController::currentNotationSelection() const
{
    return currentNotationInteraction() ? currentNotationInteraction()->selection() : nullptr;
}

INotationElementsPtr NotationActionController::currentNotationElements() const
{
    return currentNotation() ? currentNotation()->elements() : nullptr;
}

mu::async::Notification NotationActionController::currentNotationChanged() const
{
    return globalContext()->currentMasterNotationChanged();
}

INotationNoteInputPtr NotationActionController::currentNotationNoteInput() const
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return nullptr;
    }

    return interaction->noteInput();
}

mu::async::Notification NotationActionController::currentNotationNoteInputChanged() const
{
    return m_currentNotationNoteInputChanged;
}

INotationUndoStackPtr NotationActionController::currentNotationUndoStack() const
{
    auto notation = currentNotation();
    if (!notation) {
        return nullptr;
    }

    return notation->undoStack();
}

INotationStylePtr NotationActionController::currentNotationStyle() const
{
    auto notation = currentNotation();
    if (!notation) {
        return nullptr;
    }

    return notation->style();
}

mu::async::Notification NotationActionController::currentNotationStyleChanged() const
{
    return currentNotationStyle() ? currentNotationStyle()->styleChanged() : async::Notification();
}

void NotationActionController::resetState()
{
    if (playbackController()->isPlaying()) {
        playbackController()->reset();
    }

    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    if (noteInput->isNoteInputMode()) {
        noteInput->endNoteInput();
        return;
    }

    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    if (interaction->isDragStarted()) {
        interaction->endDrag();
        return;
    }

    if (interaction->isTextEditingStarted()) {
        interaction->endEditText();
        return;
    }

    if (!interaction->selection()->isNone()) {
        interaction->clearSelection();
    }
}

void NotationActionController::toggleNoteInput()
{
    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    if (noteInput->isNoteInputMode()) {
        noteInput->endNoteInput();
    } else {
        noteInput->startNoteInput();
    }
}

void NotationActionController::toggleNoteInputMethod(NoteInputMethod method)
{
    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    if (!noteInput->isNoteInputMode()) {
        noteInput->startNoteInput();
    } else if (noteInput->state().method == method) {
        noteInput->endNoteInput();
        return;
    }

    noteInput->toggleNoteInputMethod(method);
}

void NotationActionController::addNote(NoteName note, NoteAddingMode addingMode)
{
    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    if (!noteInput->isNoteInputMode()) {
        noteInput->startNoteInput();
    }

    noteInput->addNote(note, addingMode);

    playSelectedElement();
}

void NotationActionController::addText(TextType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addText(type);
}

void NotationActionController::addFiguredBass()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addFiguredBass();
}

void NotationActionController::padNote(const Pad& pad)
{
    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    startNoteInputIfNeed();

    noteInput->padNote(pad);
}

void NotationActionController::putNote(const actions::ActionData& data)
{
    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    IF_ASSERT_FAILED(data.count() > 2) {
        return;
    }

    QPoint pos = data.arg<QPoint>(0);
    bool replace = data.arg<bool>(1);
    bool insert = data.arg<bool>(2);

    noteInput->putNote(pos, replace, insert);

    playSelectedElement();
}

void NotationActionController::toggleVisible()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->toggleVisible();
}

void NotationActionController::toggleAccidental(AccidentalType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = currentNotationNoteInput();
    if (!noteInput) {
        return;
    }

    startNoteInputIfNeed();

    if (noteInput->isNoteInputMode()) {
        noteInput->setAccidental(type);
    } else {
        interaction->addAccidentalToSelection(type);
    }
}

void NotationActionController::putRestToSelection()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->putRestToSelection();
}

void NotationActionController::putRest(DurationType duration)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->putRest(duration);
}

void NotationActionController::addArticulation(SymbolId articulationSymbolId)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    startNoteInputIfNeed();

    if (noteInput->isNoteInputMode()) {
        noteInput->setArticulation(articulationSymbolId);
    } else {
        interaction->changeSelectedNotesArticulation(articulationSymbolId);
    }
}

void NotationActionController::putTuplet(int tupletCount)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    TupletOptions options;
    options.ratio.setNumerator(tupletCount);

    if (noteInput->isNoteInputMode()) {
        noteInput->addTuplet(options);
    } else {
        interaction->addTupletToSelectedChordRests(options);
    }
}

void NotationActionController::addBeamToSelectedChordRests(BeamMode mode)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addBeamToSelectedChordRests(mode);
}

void NotationActionController::addBracketsToSelection(BracketsType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addBracketsToSelection(type);
}

void NotationActionController::moveChordRestToStaff(MoveDirection direction)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }
    interaction->moveChordRestToStaff(direction);
}

void NotationActionController::moveAction(const actions::ActionCode& actionCode)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    //! NOTE Should work for any elements
    if ("next-element" == actionCode) {
        interaction->moveSelection(MoveDirection::Right, MoveSelectionType::EngravingItem);
        return;
    } else if ("prev-element" == actionCode) {
        interaction->moveSelection(MoveDirection::Left, MoveSelectionType::EngravingItem);
        return;
    }
    //! -----

    std::vector<EngravingItem*> selectionElements = interaction->selection()->elements();
    if (selectionElements.empty()) {
        LOGW() << "no selection element";
        return;
    }
    EngravingItem* element = selectionElements.back();

    if (element->isLyrics()) {
        NOT_IMPLEMENTED;
    } else if (element->isTextBase()) {
        moveText(interaction, actionCode);
    } else {
        if ("pitch-up" == actionCode) {
            if (element->isRest()) {
                NOT_IMPLEMENTED << actionCode << ", element->isRest";
            } else {
                interaction->movePitch(MoveDirection::Up, PitchMode::CHROMATIC);
            }
        } else if ("pitch-down" == actionCode) {
            if (element->isRest()) {
                NOT_IMPLEMENTED << actionCode << ", element->isRest";
            } else {
                interaction->movePitch(MoveDirection::Down, PitchMode::CHROMATIC);
            }
        } else if ("pitch-up-octave" == actionCode) {
            interaction->movePitch(MoveDirection::Up, PitchMode::OCTAVE);
        } else if ("pitch-down-octave" == actionCode) {
            interaction->movePitch(MoveDirection::Down, PitchMode::OCTAVE);
        } else if ("next-chord" == actionCode) {
            interaction->moveSelection(MoveDirection::Right, MoveSelectionType::Chord);
        } else if ("prev-chord" == actionCode) {
            interaction->moveSelection(MoveDirection::Left, MoveSelectionType::Chord);
        } else if ("next-measure" == actionCode) {
            interaction->moveSelection(MoveDirection::Right, MoveSelectionType::Measure);
        } else if ("prev-measure" == actionCode) {
            interaction->moveSelection(MoveDirection::Left, MoveSelectionType::Measure);
        } else if ("next-track" == actionCode) {
            interaction->moveSelection(MoveDirection::Right, MoveSelectionType::Track);
        } else if ("prev-track" == actionCode) {
            interaction->moveSelection(MoveDirection::Left, MoveSelectionType::Track);
        } else {
            NOT_SUPPORTED << actionCode;
        }

        playSelectedElement();
    }
}

void NotationActionController::moveWithinChord(MoveDirection direction)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->moveChordNoteSelection(direction);

    playSelectedElement(false);
}

void NotationActionController::selectTopOrBottomOfChord(MoveDirection direction)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->selectTopOrBottomOfChord(direction);

    playSelectedElement(false);
}

void NotationActionController::moveText(INotationInteractionPtr interaction, const actions::ActionCode& actionCode)
{
    MoveDirection direction = MoveDirection::Undefined;
    bool quickly = false;

    if ("next-chord" == actionCode) {
        direction = MoveDirection::Right;
    } else if ("next-measure" == actionCode) {
        direction = MoveDirection::Right;
        quickly = true;
    } else if ("prev-chord" == actionCode) {
        direction = MoveDirection::Left;
    } else if ("prev-measure" == actionCode) {
        direction = MoveDirection::Left;
        quickly = true;
    } else if ("pitch-up" == actionCode) {
        direction = MoveDirection::Up;
    } else if ("pitch-down" == actionCode) {
        direction = MoveDirection::Down;
    } else if ("pitch-up-octave" == actionCode) {
        direction = MoveDirection::Up;
        quickly = true;
    } else if ("pitch-down-octave" == actionCode) {
        direction = MoveDirection::Down;
        quickly = true;
    } else {
        NOT_SUPPORTED << actionCode;
        return;
    }

    interaction->moveText(direction, quickly);
}

void NotationActionController::increaseDecreaseDuration(int steps, bool stepByDots)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->increaseDecreaseDuration(steps, stepByDots);
}

void NotationActionController::swapVoices(int voiceIndex1, int voiceIndex2)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->swapVoices(voiceIndex1, voiceIndex2);
}

void NotationActionController::changeVoice(int voiceIndex)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    startNoteInputIfNeed();

    noteInput->setCurrentVoiceIndex(voiceIndex);

    if (!noteInput->isNoteInputMode()) {
        interaction->changeSelectedNotesVoice(voiceIndex);
    }
}

void NotationActionController::cutSelection()
{
    copySelection();
    deleteSelection();
}

void NotationActionController::copySelection()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->copySelection();
}

void NotationActionController::pasteSelection(PastingType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    Fraction scale = resolvePastingScale(interaction, type);
    interaction->pasteSelection(scale);
}

Fraction NotationActionController::resolvePastingScale(const INotationInteractionPtr& interaction, PastingType type) const
{
    const Fraction DEFAULT_SCALE(1, 1);

    switch (type) {
    case PastingType::Default: return DEFAULT_SCALE;
    case PastingType::Half: return Fraction(1, 2);
    case PastingType::Double: return Fraction(2, 1);
    case PastingType::Special:
        Fraction scale = DEFAULT_SCALE;
        Fraction duration = interaction->noteInput()->state().duration.fraction();

        if (duration.isValid() && !duration.isZero()) {
            scale = duration * 4;
            scale.reduce();
        }

        return scale;
    }

    return DEFAULT_SCALE;
}

void NotationActionController::deleteSelection()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->deleteSelection();
}

void NotationActionController::swapSelection()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->swapSelection();
}

void NotationActionController::flipSelection()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->flipSelection();
}

void NotationActionController::addTie()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    if (noteInput->isNoteInputMode()) {
        noteInput->addTie();
    } else {
        interaction->addTieToSelection();
    }
}

void NotationActionController::chordTie()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    if (noteInput->isNoteInputMode()) {
        noteInput->addTie();
    } else {
        interaction->addTiedNoteToChord();
    }
}

void NotationActionController::addSlur()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    if (noteInput->isNoteInputMode() && noteInput->state().withSlur) {
        noteInput->resetSlur();
    } else {
        interaction->addSlurToSelection();
    }
}

void NotationActionController::addInterval(int interval)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addIntervalToSelectedNotes(interval);
}

void NotationActionController::addFret(int fretIndex)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addFret(fretIndex);
}

void NotationActionController::undo()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }
    interaction->undo();
}

void NotationActionController::redo()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }
    interaction->redo();
}

void NotationActionController::addChordToSelection(MoveDirection direction)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    if (interaction->selection()->elements().size() == 1
        && interaction->selection()->elements().front()->type() == ElementType::SLUR) {
        return;
    }

    interaction->addChordToSelection(direction);
}

void NotationActionController::selectAllSimilarElements()
{
    auto notationElements = currentNotationElements();
    auto interaction = currentNotationInteraction();
    if (!notationElements || !interaction) {
        return;
    }

    EngravingItem* selectedElement = interaction->selection()->element();
    if (!selectedElement) {
        return;
    }

    FilterElementsOptions options = elementsFilterOptions(selectedElement);
    std::vector<EngravingItem*> elements = notationElements->elements(options);
    if (elements.empty()) {
        return;
    }

    interaction->clearSelection();

    interaction->select(elements, SelectType::ADD);
}

void NotationActionController::selectAllSimilarElementsInStaff()
{
    auto notationElements = currentNotationElements();
    auto interaction = currentNotationInteraction();
    if (!notationElements || !interaction) {
        return;
    }

    EngravingItem* selectedElement = interaction->selection()->element();
    if (!selectedElement) {
        return;
    }

    FilterElementsOptions options = elementsFilterOptions(selectedElement);
    options.staffStart = selectedElement->staffIdx();
    options.staffEnd = options.staffStart + 1;

    std::vector<EngravingItem*> elements = notationElements->elements(options);
    if (elements.empty()) {
        return;
    }

    interaction->clearSelection();

    interaction->select(elements, SelectType::ADD);
}

void NotationActionController::selectAllSimilarElementsInRange()
{
    NOT_IMPLEMENTED;
}

void NotationActionController::selectSection()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->selectSection();
}

void NotationActionController::firstElement()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->selectFirstElement();
}

void NotationActionController::lastElement()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->selectLastElement();
}

void NotationActionController::openSelectionMoreOptions()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto selection = interaction->selection();
    if (!selection) {
        return;
    }

    bool noteSelected = selection->element() && selection->element()->isNote();

    if (noteSelected) {
        interactive()->open("musescore://notation/selectnote");
    } else {
        interactive()->open("musescore://notation/selectelement");
    }
}

void NotationActionController::selectAll()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->selectAll();
}

void NotationActionController::toggleLayoutBreak(LayoutBreakType breakType)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->toggleLayoutBreak(breakType);
}

void NotationActionController::splitMeasure()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->splitSelectedMeasure();
}

void NotationActionController::joinSelectedMeasures()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->joinSelectedMeasures();
}

void NotationActionController::insertBoxes(BoxType boxType, int count)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    int firstSelectedBoxIndex = this->firstSelectedBoxIndex();

    if (firstSelectedBoxIndex == INVALID_BOX_INDEX) {
        return;
    }

    interaction->addBoxes(boxType, count, firstSelectedBoxIndex);
}

void NotationActionController::insertBox(BoxType boxType)
{
    insertBoxes(boxType, 1);
}

int NotationActionController::firstSelectedBoxIndex() const
{
    int result = INVALID_BOX_INDEX;

    auto selection = currentNotationSelection();
    if (!selection) {
        return result;
    }

    if (selection->isRange()) {
        result = selection->range()->startMeasureIndex();
    } else if (selection->element()) {
        Measure* measure = selection->element()->findMeasure();
        result = measure ? measure->index() : INVALID_BOX_INDEX;
    }

    return result;
}

void NotationActionController::appendBoxes(BoxType boxType, int count)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addBoxes(boxType, count);
}

void NotationActionController::appendBox(BoxType boxType)
{
    appendBoxes(boxType, 1);
}

void NotationActionController::addOttava(OttavaType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addOttavaToSelection(type);
}

void NotationActionController::addHairpin(HairpinType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addHairpinToSelection(type);
}

void NotationActionController::addAnchoredNoteLine()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addAnchoredLineToSelectedNotes();
}

void NotationActionController::explodeSelectedStaff()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->explodeSelectedStaff();
}

void NotationActionController::implodeSelectedStaff()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->implodeSelectedStaff();
}

void NotationActionController::realizeSelectedChordSymbols()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->realizeSelectedChordSymbols();
}

void NotationActionController::removeSelectedRange()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->removeSelectedRange();
}

void NotationActionController::removeEmptyTrailingMeasures()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->removeEmptyTrailingMeasures();
}

void NotationActionController::fillSelectionWithSlashes()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->fillSelectionWithSlashes();
}

void NotationActionController::replaceSelectedNotesWithSlashes()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->replaceSelectedNotesWithSlashes();
}

void NotationActionController::spellPitches()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->spellPitches();
}

void NotationActionController::regroupNotesAndRests()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->regroupNotesAndRests();
}

void NotationActionController::resequenceRehearsalMarks()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->resequenceRehearsalMarks();
}

void NotationActionController::unrollRepeats()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->unrollRepeats();
}

void NotationActionController::copyLyrics()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->copyLyrics();
}

void NotationActionController::addGraceNotesToSelectedNotes(GraceNoteType type)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->addGraceNotesToSelectedNotes(type);
}

void NotationActionController::changeEnharmonicSpellingBoth()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->changeEnharmonicSpelling(true);
}

void NotationActionController::changeEnharmonicSpellingCurrent()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->changeEnharmonicSpelling(false);
}

void NotationActionController::addStretch(qreal value)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto selection = currentNotationSelection();
    if (!selection) {
        return;
    }

    if (!selection->isRange()) {
        return;
    }

    interaction->addStretch(value);
}

void NotationActionController::resetStretch()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto selection = currentNotationSelection();
    if (!selection) {
        return;
    }

    if (!selection->isRange()) {
        return;
    }

    interaction->resetToDefault(ResettableValueType::Stretch);
}

void NotationActionController::resetTextStyleOverrides()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    interaction->resetToDefault(ResettableValueType::TextStyleOverriders);
}

void NotationActionController::resetBeamMode()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto selection = currentNotationSelection();
    if (!selection) {
        return;
    }

    if (selection->isNone() || selection->isRange()) {
        interaction->resetToDefault(ResettableValueType::BeamMode);
    }
}

void NotationActionController::resetShapesAndPosition()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto selection = currentNotationSelection();
    if (!selection) {
        return;
    }

    if (selection->isNone()) {
        return;
    }

    interaction->resetToDefault(ResettableValueType::TextStyleOverriders);
}

void NotationActionController::selectMeasuresCountAndInsert()
{
    RetVal<Val> measureCount = interactive()->open("musescore://notation/selectmeasurescount?operation=insert");

    if (measureCount.ret) {
        insertBoxes(BoxType::Measure, measureCount.val.toInt());
    }
}

void NotationActionController::selectMeasuresCountAndAppend()
{
    RetVal<Val> measureCount = interactive()->open("musescore://notation/selectmeasurescount?operation=append");

    if (measureCount.ret) {
        appendBoxes(BoxType::Measure, measureCount.val.toInt());
    }
}

void NotationActionController::openEditStyleDialog()
{
    interactive()->open("musescore://notation/style");
}

void NotationActionController::openPageSettingsDialog()
{
    interactive()->open("musescore://notation/pagesettings");
}

void NotationActionController::openStaffProperties()
{
    interactive()->open("musescore://notation/staffproperties");
}

void NotationActionController::openBreaksDialog()
{
    interactive()->open("musescore://notation/breaks");
}

void NotationActionController::openScoreProperties()
{
    interactive()->open("musescore://notation/properties");
}

void NotationActionController::openTransposeDialog()
{
    interactive()->open("musescore://notation/transpose");
}

void NotationActionController::openPartsDialog()
{
    interactive()->open("musescore://notation/parts");
}

FilterElementsOptions NotationActionController::elementsFilterOptions(const EngravingItem* element) const
{
    FilterElementsOptions options;
    options.elementType = element->type();

    if (element->type() == ElementType::NOTE) {
        const Ms::Note* note = dynamic_cast<const Ms::Note*>(element);
        if (note->chord()->isGrace()) {
            options.subtype = -1;
        } else {
            options.subtype = element->subtype();
        }
    }

    return options;
}

bool NotationActionController::isEditingText() const
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return false;
    }

    return interaction->isTextEditingStarted();
}

bool NotationActionController::isEditingLyrics() const
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return false;
    }

    return interaction->isTextEditingStarted() && interaction->selection()->element()
           && interaction->selection()->element()->isLyrics();
}

void NotationActionController::openTupletOtherDialog()
{
    interactive()->open("musescore://notation/othertupletdialog");
}

void NotationActionController::openStaffTextPropertiesDialog()
{
    interactive()->open("musescore://notation/stafftextproperties");
}

void NotationActionController::openMeasurePropertiesDialog()
{
    interactive()->open("musescore://notation/measureproperties");
}

void NotationActionController::toggleScoreConfig(ScoreConfigType configType)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    ScoreConfig config = interaction->scoreConfig();

    switch (configType) {
    case ScoreConfigType::ShowInvisibleElements:
        config.isShowInvisibleElements = !config.isShowInvisibleElements;
        break;
    case ScoreConfigType::ShowUnprintableElements:
        config.isShowUnprintableElements = !config.isShowUnprintableElements;
        break;
    case ScoreConfigType::ShowFrames:
        config.isShowFrames = !config.isShowFrames;
        break;
    case ScoreConfigType::ShowPageMargins:
        config.isShowPageMargins = !config.isShowPageMargins;
        break;
    case ScoreConfigType::MarkIrregularMeasures:
        config.isMarkIrregularMeasures = !config.isMarkIrregularMeasures;
        break;
    }

    interaction->setScoreConfig(config);
    interaction->scoreConfigChanged().send(configType);
}

void NotationActionController::toggleConcertPitch()
{
    INotationStylePtr style = currentNotationStyle();
    if (!style) {
        return;
    }

    currentNotationUndoStack()->prepareChanges();
    bool enabled = style->styleValue(StyleId::concertPitch).toBool();
    style->setStyleValue(StyleId::concertPitch, !enabled);
    currentNotationUndoStack()->commitChanges();
}

void NotationActionController::playSelectedElement(bool playChord)
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    EngravingItem* element = interaction->selection()->element();
    if (!element || !element->isNote()) {
        return;
    }

    if (playChord && playbackConfiguration()->playChordWhenEditing()) {
        element = element->elementBase();
    }

    playbackController()->playElement(element);
}

void NotationActionController::startNoteInputIfNeed()
{
    auto interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }

    auto noteInput = interaction->noteInput();
    if (!noteInput) {
        return;
    }

    if (interaction->selection()->isNone() && !noteInput->isNoteInputMode()) {
        noteInput->startNoteInput();
    }
}

bool NotationActionController::hasSelection() const
{
    return currentNotationSelection() ? !currentNotationSelection()->isNone() : false;
}

bool NotationActionController::canUndo() const
{
    return currentNotationUndoStack() ? currentNotationUndoStack()->canUndo() : false;
}

bool NotationActionController::canRedo() const
{
    return currentNotationUndoStack() ? currentNotationUndoStack()->canRedo() : false;
}

bool NotationActionController::isNotationPage() const
{
    return interactive()->isOpened("musescore://notation").val;
}

bool NotationActionController::isStandardStaff() const
{
    return isNotEditingText() && !isTablatureStaff();
}

bool NotationActionController::isTablatureStaff() const
{
    return isNotEditingText() && currentNotation()->elements()->msScore()->inputState().staffGroup() == Ms::StaffGroup::TAB;
}

bool NotationActionController::isNotEditingText() const
{
    return !isEditingText();
}

void NotationActionController::nextLyrics()
{
    currentNotationInteraction()->nextLyrics();
}

void NotationActionController::previousLyrics()
{
    currentNotationInteraction()->nextLyrics(true);
}

void NotationActionController::nextLyricsVerse()
{
    currentNotationInteraction()->nextLyricsVerse();
}

void NotationActionController::previousLyricsVerse()
{
    currentNotationInteraction()->nextLyricsVerse(true);
}

void NotationActionController::nextSyllable()
{
    currentNotationInteraction()->nextSyllable();
}

void NotationActionController::addMelisma()
{
    currentNotationInteraction()->addMelisma();
}

void NotationActionController::addLyricsVerse()
{
    currentNotationInteraction()->addLyricsVerse();
}

void NotationActionController::toggleBold()
{
    currentNotationInteraction()->toggleBold();
}

void NotationActionController::toggleItalic()
{
    currentNotationInteraction()->toggleItalic();
}

void NotationActionController::toggleUnderline()
{
    currentNotationInteraction()->toggleUnderline();
}

void NotationActionController::registerAction(const mu::actions::ActionCode& code,
                                              std::function<void()> handler, bool (NotationActionController::* isEnabled)() const)
{
    m_isEnabledMap[code] = isEnabled;
    dispatcher()->reg(this, code, handler);
}

void NotationActionController::registerAction(const mu::actions::ActionCode& code,
                                              void (NotationActionController::* handler)(const actions::ActionData& data),
                                              bool (NotationActionController::* isEnabled)() const)
{
    m_isEnabledMap[code] = isEnabled;
    dispatcher()->reg(this, code, this, handler);
}

void NotationActionController::registerAction(const mu::actions::ActionCode& code,
                                              void (NotationActionController::* handler)(),
                                              bool (NotationActionController::* isEnabled)() const)
{
    m_isEnabledMap[code] = isEnabled;
    dispatcher()->reg(this, code, this, handler);
}

void NotationActionController::registerNoteInputAction(const mu::actions::ActionCode& code, NoteInputMethod inputMethod)
{
    registerAction(code, [this, inputMethod]() { toggleNoteInputMethod(inputMethod); });
}

void NotationActionController::registerNoteAction(const mu::actions::ActionCode& code, NoteName noteName, NoteAddingMode addingMode)
{
    registerAction(code, [this, noteName, addingMode]() { addNote(noteName, addingMode); }, &NotationActionController::isStandardStaff);
}

void NotationActionController::registerPadNoteAction(const mu::actions::ActionCode& code, Pad padding)
{
    registerAction(code, [this, padding]() { padNote(padding); });
}

void NotationActionController::registerTextAction(const mu::actions::ActionCode& code, void (NotationActionController::* handler)())
{
    registerAction(code, handler, &NotationActionController::isEditingText);
}

void NotationActionController::registerLyricsAction(const mu::actions::ActionCode& code, void (NotationActionController::* handler)())
{
    registerAction(code, handler, &NotationActionController::isEditingLyrics);
}

void NotationActionController::registerMoveAction(const mu::actions::ActionCode& code)
{
    registerAction(code, [this, code]() { moveAction(code); });
}
