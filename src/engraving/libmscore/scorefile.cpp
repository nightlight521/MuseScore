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

#include <cmath>
#include <QDir>
#include <QBuffer>
#include <QRegularExpression>

#include "style/defaultstyle.h"

#include "compat/chordlist.h"
#include "compat/writescorehook.h"
#include "io/xml.h"

#include "score.h"
#include "engravingitem.h"
#include "measure.h"
#include "segment.h"
#include "slur.h"
#include "chordrest.h"
#include "chord.h"
#include "tuplet.h"
#include "beam.h"
#include "revisions.h"
#include "page.h"
#include "part.h"
#include "staff.h"
#include "system.h"
#include "keysig.h"
#include "clef.h"
#include "text.h"
#include "ottava.h"
#include "volta.h"
#include "excerpt.h"
#include "mscore.h"
#include "stafftype.h"
#include "sym.h"
#include "scoreorder.h"
#include "utils.h"
#include "sig.h"
#include "undo.h"
#include "imageStore.h"
#include "audio.h"
#include "barline.h"
#include "masterscore.h"
#include "factory.h"

#include "log.h"
#include "config.h"

using namespace mu;
using namespace mu::engraving;

namespace Ms {
//---------------------------------------------------------
//   writeMeasure
//---------------------------------------------------------

static void writeMeasure(XmlWriter& xml, MeasureBase* m, int staffIdx, bool writeSystemElements, bool forceTimeSig)
{
    //
    // special case multi measure rest
    //
    if (m->isMeasure() || staffIdx == 0) {
        m->write(xml, staffIdx, writeSystemElements, forceTimeSig);
    }

    if (m->score()->styleB(Sid::createMultiMeasureRests) && m->isMeasure() && toMeasure(m)->mmRest()) {
        toMeasure(m)->mmRest()->write(xml, staffIdx, writeSystemElements, forceTimeSig);
    }

    xml.setCurTick(m->endTick());
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Score::write(XmlWriter& xml, bool selectionOnly, compat::WriteScoreHook& hook)
{
    // if we have multi measure rests and some parts are hidden,
    // then some layout information is missing:
    // relayout with all parts set visible

    QList<Part*> hiddenParts;
    bool unhide = false;
    if (styleB(Sid::createMultiMeasureRests)) {
        for (Part* part : qAsConst(_parts)) {
            if (!part->show()) {
                if (!unhide) {
                    startCmd();
                    unhide = true;
                }
                part->undoChangeProperty(Pid::VISIBLE, true);
                hiddenParts.append(part);
            }
        }
    }
    if (unhide) {
        doLayout();
        for (Part* p : hiddenParts) {
            p->setShow(false);
        }
    }

    xml.stag(this);
    if (excerpt()) {
        Excerpt* e = excerpt();
        QMultiMap<int, int> trackList = e->tracks();
        QMapIterator<int, int> i(trackList);
        if (!(trackList.size() == e->nstaves() * VOICES) && !trackList.isEmpty()) {
            while (i.hasNext()) {
                i.next();
                xml.tagE(QString("Tracklist sTrack=\"%1\" dstTrack=\"%2\"").arg(i.key()).arg(i.value()));
            }
        }
    }

    if (lineMode()) {
        xml.tag("layoutMode", "line");
    }
    if (systemMode()) {
        xml.tag("layoutMode", "system");
    }

    if (_audio && xml.isMsczMode()) {
        xml.tag("playMode", int(_playMode));
        _audio->write(xml);
    }

    for (int i = 0; i < 32; ++i) {
        if (!_layerTags[i].isEmpty()) {
            xml.tag(QString("LayerTag id=\"%1\" tag=\"%2\"").arg(i).arg(_layerTags[i]),
                    _layerTagComments[i]);
        }
    }
    int n = _layer.size();
    for (int i = 1; i < n; ++i) {         // don’t save default variant
        const Layer& l = _layer[i];
        xml.tagE(QString("Layer name=\"%1\" mask=\"%2\"").arg(l.name).arg(l.tags));
    }
    xml.tag("currentLayer", _currentLayer);

    if (isMaster() && !MScore::testMode) {
        _synthesizerState.write(xml);
    }

    if (pageNumberOffset()) {
        xml.tag("page-offset", pageNumberOffset());
    }
    xml.tag("Division", MScore::division);
    xml.setCurTrack(-1);

    hook.onWriteStyle302(this, xml);

    xml.tag("showInvisible",   _showInvisible);
    xml.tag("showUnprintable", _showUnprintable);
    xml.tag("showFrames",      _showFrames);
    xml.tag("showMargins",     _showPageborders);
    xml.tag("markIrregularMeasures", _markIrregularMeasures, true);

    QMapIterator<QString, QString> i(_metaTags);
    while (i.hasNext()) {
        i.next();
        // do not output "platform" and "creationDate" in test and save template mode
        if ((!MScore::testMode && !MScore::saveTemplateMode) || (i.key() != "platform" && i.key() != "creationDate")) {
            xml.tag(QString("metaTag name=\"%1\"").arg(i.key().toHtmlEscaped()), i.value());
        }
    }

    if (_scoreOrder.isValid()) {
        ScoreOrder order = _scoreOrder;
        order.updateInstruments(this);
        order.write(xml);
    }

    xml.setCurTrack(0);
    int staffStart;
    int staffEnd;
    MeasureBase* measureStart;
    MeasureBase* measureEnd;

    if (selectionOnly) {
        staffStart   = _selection.staffStart();
        staffEnd     = _selection.staffEnd();
        // make sure we select full parts
        Staff* sStaff = staff(staffStart);
        Part* sPart = sStaff->part();
        Staff* eStaff = staff(staffEnd - 1);
        Part* ePart = eStaff->part();
        staffStart = staffIdx(sPart);
        staffEnd = staffIdx(ePart) + ePart->nstaves();
        measureStart = _selection.startSegment()->measure();
        if (measureStart->isMeasure() && toMeasure(measureStart)->isMMRest()) {
            measureStart = toMeasure(measureStart)->mmRestFirst();
        }
        if (_selection.endSegment()) {
            measureEnd   = _selection.endSegment()->measure()->next();
        } else {
            measureEnd   = 0;
        }
    } else {
        staffStart   = 0;
        staffEnd     = nstaves();
        measureStart = first();
        measureEnd   = 0;
    }

    // Let's decide: write midi mapping to a file or not
    masterScore()->checkMidiMapping();
    for (const Part* part : qAsConst(_parts)) {
        if (!selectionOnly || ((staffIdx(part) >= staffStart) && (staffEnd >= staffIdx(part) + part->nstaves()))) {
            part->write(xml);
        }
    }

    xml.setCurTrack(0);
    xml.setTrackDiff(-staffStart * VOICES);
    if (measureStart) {
        for (int staffIdx = staffStart; staffIdx < staffEnd; ++staffIdx) {
            xml.stag(staff(staffIdx), QString("id=\"%1\"").arg(staffIdx + 1 - staffStart));
            xml.setCurTick(measureStart->tick());
            xml.setTickDiff(xml.curTick());
            xml.setCurTrack(staffIdx * VOICES);
            bool writeSystemElements = (staffIdx == staffStart);
            bool firstMeasureWritten = false;
            bool forceTimeSig = false;
            for (MeasureBase* m = measureStart; m != measureEnd; m = m->next()) {
                // force timesig if first measure and selectionOnly
                if (selectionOnly && m->isMeasure()) {
                    if (!firstMeasureWritten) {
                        forceTimeSig = true;
                        firstMeasureWritten = true;
                    } else {
                        forceTimeSig = false;
                    }
                }
                writeMeasure(xml, m, staffIdx, writeSystemElements, forceTimeSig);
            }
            xml.etag();
        }
    }
    xml.setCurTrack(-1);

    hook.onWriteExcerpts302(this, xml, selectionOnly);

    xml.etag();

    if (unhide) {
        endCmd(true);
    }
}

//---------------------------------------------------------
// linkMeasures
//---------------------------------------------------------

void Score::linkMeasures(Score* score)
{
    MeasureBase* mbMaster = score->first();
    for (MeasureBase* mb = first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        while (mbMaster && !mbMaster->isMeasure()) {
            mbMaster = mbMaster->next();
        }
        if (!mbMaster) {
            qDebug("Measures in MasterScore and Score are not in sync.");
            break;
        }
        mb->linkTo(mbMaster);
        mbMaster = mbMaster->next();
    }
}

//---------------------------------------------------------
//   Staff
//---------------------------------------------------------

void Score::readStaff(XmlReader& e)
{
    int staff = e.intAttribute("id", 1) - 1;
    int measureIdx = 0;
    e.setCurrentMeasureIndex(0);
    e.setTick(Fraction(0, 1));
    e.setTrack(staff * VOICES);

    if (staff == 0) {
        while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());

            if (tag == "Measure") {
                Measure* measure = nullptr;
                measure = new Measure(this->dummy()->system());
                measure->setTick(e.tick());
                e.setCurrentMeasureIndex(measureIdx++);
                //
                // inherit timesig from previous measure
                //
                Measure* m = e.lastMeasure();         // measure->prevMeasure();
                Fraction f(m ? m->timesig() : Fraction(4, 4));
                measure->setTicks(f);
                measure->setTimesig(f);

                measure->read(e, staff);
                measure->checkMeasure(staff);
                if (!measure->isMMRest()) {
                    measures()->add(measure);
                    e.setLastMeasure(measure);
                    e.setTick(measure->tick() + measure->ticks());
                } else {
                    // this is a multi measure rest
                    // always preceded by the first measure it replaces
                    Measure* m1 = e.lastMeasure();

                    if (m1) {
                        m1->setMMRest(measure);
                        measure->setTick(m1->tick());
                    }
                }
            } else if (tag == "HBox" || tag == "VBox" || tag == "TBox" || tag == "FBox") {
                MeasureBase* mb = toMeasureBase(Factory::createItemByName(tag, this->dummy()));
                mb->read(e);
                mb->setTick(e.tick());
                measures()->add(mb);
            } else if (tag == "tick") {
                e.setTick(Fraction::fromTicks(fileDivision(e.readInt())));
            } else {
                e.unknown();
            }
        }
    } else {
        Measure* measure = firstMeasure();
        while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());

            if (tag == "Measure") {
                if (measure == 0) {
                    qDebug("Score::readStaff(): missing measure!");
                    measure = new Measure(this->dummy()->system());
                    measure->setTick(e.tick());
                    measures()->add(measure);
                }
                e.setTick(measure->tick());
                e.setCurrentMeasureIndex(measureIdx++);
                measure->read(e, staff);
                measure->checkMeasure(staff);
                if (measure->isMMRest()) {
                    measure = e.lastMeasure()->nextMeasure();
                } else {
                    e.setLastMeasure(measure);
                    if (measure->mmRest()) {
                        measure = measure->mmRest();
                    } else {
                        measure = measure->nextMeasure();
                    }
                }
            } else if (tag == "tick") {
                e.setTick(Fraction::fromTicks(fileDivision(e.readInt())));
            } else {
                e.unknown();
            }
        }
    }
}

//---------------------------------------------------------
//   createThumbnail
//---------------------------------------------------------

std::shared_ptr<mu::draw::Pixmap> Score::createThumbnail()
{
    LayoutMode mode = layoutMode();
    setLayoutMode(LayoutMode::PAGE);
    doLayout();

    Page* page = pages().at(0);
    RectF fr  = page->abbox();
    qreal mag  = 256.0 / qMax(fr.width(), fr.height());
    int w      = int(fr.width() * mag);
    int h      = int(fr.height() * mag);

    int dpm = lrint(DPMM * 1000.0);

    auto pixmap = imageProvider()->createPixmap(w, h, dpm, mu::draw::Color(255, 255, 255, 255));

    double pr = MScore::pixelRatio;
    MScore::pixelRatio = 1.0;

    auto painterProvider = imageProvider()->painterForImage(pixmap);
    mu::draw::Painter p(painterProvider, "thumbnail");

    p.setAntialiasing(true);
    p.scale(mag, mag);
    print(&p, 0);
    p.endDraw();

    MScore::pixelRatio = pr;

    if (layoutMode() != mode) {
        setLayoutMode(mode);
        doLayout();
    }
    return pixmap;
}

//---------------------------------------------------------
//   loadStyle
//---------------------------------------------------------

bool Score::loadStyle(const QString& fn, bool ign, const bool overlap)
{
    QFile f(fn);
    if (f.open(QIODevice::ReadOnly)) {
        MStyle st = style();
        if (st.read(&f, ign)) {
            undo(new ChangeStyle(this, st, overlap));
            return true;
        } else {
            MScore::lastError = QObject::tr("The style file is not compatible with this version of MuseScore.");
            return false;
        }
    }
    MScore::lastError = strerror(errno);
    return false;
}

//---------------------------------------------------------
//   saveStyle
//---------------------------------------------------------

bool Score::saveStyle(const QString& name)
{
    QString ext(".mss");
    QFileInfo info(name);

    if (info.suffix().isEmpty()) {
        info.setFile(info.filePath() + ext);
    }
    QFile f(info.filePath());
    if (!f.open(QIODevice::WriteOnly)) {
        MScore::lastError = tr("Open Style File\n%1\nfailed: %2").arg(info.filePath(), strerror(errno));
        return false;
    }

    bool ok = style().write(&f);
    if (!ok) {
        MScore::lastError = QObject::tr("Write Style failed: %1").arg(f.errorString());
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   saveFile
//    return true on success
//---------------------------------------------------------

//! FIXME
//extern QString revision;
static QString revision;

bool Score::writeScore(QIODevice* f, bool msczFormat, bool onlySelection, compat::WriteScoreHook& hook)
{
    XmlWriter xml(this, f);
    xml.setIsMsczMode(msczFormat);
    xml.header();

    xml.stag("museScore version=\"" MSC_VERSION "\"");

    if (!MScore::testMode) {
        xml.tag("programVersion", VERSION);
        xml.tag("programRevision", revision);
    }
    write(xml, onlySelection, hook);
    xml.etag();
    if (isMaster()) {
        masterScore()->revisions()->write(xml);
    }
    if (!onlySelection) {
        //update version values for i.e. plugin access
        _mscoreVersion = VERSION;
        _mscoreRevision = revision.toInt(0, 16);
        _mscVersion = MSCVERSION;
    }
    return true;
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------

void Score::print(mu::draw::Painter* painter, int pageNo)
{
    _printing  = true;
    MScore::pdfPrinting = true;
    Page* page = pages().at(pageNo);
    RectF fr  = page->abbox();

    QList<EngravingItem*> ell = page->items(fr);
    std::stable_sort(ell.begin(), ell.end(), elementLessThan);
    for (const EngravingItem* e : qAsConst(ell)) {
        if (!e->visible()) {
            continue;
        }
        painter->save();
        painter->translate(e->pagePos());
        e->draw(painter);
        painter->restore();
    }
    MScore::pdfPrinting = false;
    _printing = false;
}

//---------------------------------------------------------
//   writeVoiceMove
//    write <move> and starting <voice> tags to denote
//    change in position.
//    Returns true if <voice> tag was written.
//---------------------------------------------------------

static bool writeVoiceMove(XmlWriter& xml, Segment* seg, const Fraction& startTick, int track, int* lastTrackWrittenPtr)
{
    bool voiceTagWritten = false;
    int& lastTrackWritten = *lastTrackWrittenPtr;
    if ((lastTrackWritten < track) && !xml.clipboardmode()) {
        while (lastTrackWritten < (track - 1)) {
            xml.tagE("voice");
            ++lastTrackWritten;
        }
        xml.stag("voice");
        xml.setCurTick(startTick);
        xml.setCurTrack(track);
        ++lastTrackWritten;
        voiceTagWritten = true;
    }

    if ((xml.curTick() != seg->tick()) || (track != xml.curTrack())) {
        Location curr = Location::absolute();
        Location dest = Location::absolute();
        curr.setFrac(xml.curTick());
        dest.setFrac(seg->tick());
        curr.setTrack(xml.curTrack());
        dest.setTrack(track);

        dest.toRelative(curr);
        dest.write(xml);

        xml.setCurTick(seg->tick());
        xml.setCurTrack(track);
    }

    return voiceTagWritten;
}

//---------------------------------------------------------
//   writeSegments
//    ls  - write upto this segment (excluding)
//          can be zero
//---------------------------------------------------------

void Score::writeSegments(XmlWriter& xml, int strack, int etrack,
                          Segment* sseg, Segment* eseg, bool writeSystemElements, bool forceTimeSig)
{
    Fraction startTick = xml.curTick();
    Fraction endTick   = eseg ? eseg->tick() : lastMeasure()->endTick();
    bool clip          = xml.clipboardmode();

    // in clipboard mode, ls might be in an mmrest
    // since we are traversing regular measures,
    // force them out of mmRest
    if (clip) {
        Measure* lm = eseg ? eseg->measure() : 0;
        Measure* fm = sseg ? sseg->measure() : 0;
        if (lm && lm->isMMRest()) {
            lm = lm->mmRestLast();
            if (lm) {
                eseg = lm->nextMeasure() ? lm->nextMeasure()->first() : nullptr;
            } else {
                qDebug("writeSegments: no measure for end segment in mmrest");
            }
        }
        if (fm && fm->isMMRest()) {
            fm = fm->mmRestFirst();
            if (fm) {
                sseg = fm->first();
            }
        }
    }

    QList<Spanner*> spanners;
    auto sl = spannerMap().findOverlapping(sseg->tick().ticks(), endTick.ticks());
    for (auto i : sl) {
        Spanner* s = i.value;
        if (s->generated() || !xml.canWrite(s)) {
            continue;
        }
        // don't write voltas to clipboard
        if (clip && s->isVolta()) {
            continue;
        }
        spanners.push_back(s);
    }

    int lastTrackWritten = strack - 1;   // for counting necessary <voice> tags
    for (int track = strack; track < etrack; ++track) {
        if (!xml.canWriteVoice(track)) {
            continue;
        }

        bool voiceTagWritten = false;

        bool timeSigWritten = false;     // for forceTimeSig
        bool crWritten = false;          // for forceTimeSig
        bool keySigWritten = false;      // for forceTimeSig

        for (Segment* segment = sseg; segment && segment != eseg; segment = segment->next1()) {
            if (!segment->enabled()) {
                continue;
            }
            if (track == 0) {
                segment->setWritten(false);
            }
            EngravingItem* e = segment->element(track);

            //
            // special case: - barline span > 1
            //               - part (excerpt) staff starts after
            //                 barline element
            bool needMove = (segment->tick() != xml.curTick() || (track > lastTrackWritten));
            if ((segment->isEndBarLineType()) && !e && writeSystemElements && ((track % VOICES) == 0)) {
                // search barline:
                for (int idx = track - VOICES; idx >= 0; idx -= VOICES) {
                    if (segment->element(idx)) {
                        int oDiff = xml.trackDiff();
                        xml.setTrackDiff(idx);                      // staffIdx should be zero
                        segment->element(idx)->write(xml);
                        xml.setTrackDiff(oDiff);
                        break;
                    }
                }
            }
            for (EngravingItem* e1 : segment->annotations()) {
                if (e1->track() != track || e1->generated() || (e1->systemFlag() && !writeSystemElements)) {
                    continue;
                }
                if (needMove) {
                    voiceTagWritten |= writeVoiceMove(xml, segment, startTick, track, &lastTrackWritten);
                    needMove = false;
                }
                e1->write(xml);
            }
            Measure* m = segment->measure();
            // don't write spanners for multi measure rests

            if ((!(m && m->isMMRest())) && segment->isChordRestType()) {
                for (Spanner* s : spanners) {
                    if (s->track() == track) {
                        bool end = false;
                        if (s->anchor() == Spanner::Anchor::CHORD || s->anchor() == Spanner::Anchor::NOTE) {
                            end = s->tick2() < endTick;
                        } else {
                            end = s->tick2() <= endTick;
                        }
                        if (s->tick() == segment->tick() && (!clip || end) && !s->isSlur()) {
                            if (needMove) {
                                voiceTagWritten |= writeVoiceMove(xml, segment, startTick, track, &lastTrackWritten);
                                needMove = false;
                            }
                            s->writeSpannerStart(xml, segment, track);
                        }
                    }
                    if ((s->tick2() == segment->tick())
                        && !s->isSlur()
                        && (s->effectiveTrack2() == track)
                        && (!clip || s->tick() >= sseg->tick())
                        ) {
                        if (needMove) {
                            voiceTagWritten |= writeVoiceMove(xml, segment, startTick, track, &lastTrackWritten);
                            needMove = false;
                        }
                        s->writeSpannerEnd(xml, segment, track);
                    }
                }
            }

            if (!e || !xml.canWrite(e)) {
                continue;
            }
            if (e->generated()) {
                continue;
            }
            if (forceTimeSig && track2voice(track) == 0 && segment->segmentType() == SegmentType::ChordRest && !timeSigWritten
                && !crWritten) {
                // Ensure that <voice> tag is open
                voiceTagWritten |= writeVoiceMove(xml, segment, startTick, track, &lastTrackWritten);
                // we will miss a key sig!
                if (!keySigWritten) {
                    Key k = score()->staff(track2staff(track))->key(segment->tick());
                    KeySig* ks = new KeySig(this->dummy()->segment());
                    ks->setKey(k);
                    ks->write(xml);
                    delete ks;
                    keySigWritten = true;
                }
                // we will miss a time sig!
                Fraction tsf = sigmap()->timesig(segment->tick()).timesig();
                TimeSig* ts = new TimeSig(this->dummy()->segment());
                ts->setSig(tsf);
                ts->write(xml);
                delete ts;
                timeSigWritten = true;
            }
            if (needMove) {
                voiceTagWritten |= writeVoiceMove(xml, segment, startTick, track, &lastTrackWritten);
                // needMove = false; //! NOTE Not necessary, because needMove is currently never read again.
            }
            if (e->isChordRest()) {
                ChordRest* cr = toChordRest(e);
                cr->writeTupletStart(xml);
            }
            e->write(xml);

            if (e->isChordRest()) {
                ChordRest* cr = toChordRest(e);
                cr->writeTupletEnd(xml);
            }

            if (!(e->isRest() && toRest(e)->isGap())) {
                segment->write(xml);            // write only once
            }
            if (forceTimeSig) {
                if (segment->segmentType() == SegmentType::KeySig) {
                    keySigWritten = true;
                }
                if (segment->segmentType() == SegmentType::TimeSig) {
                    timeSigWritten = true;
                }
                if (segment->segmentType() == SegmentType::ChordRest) {
                    crWritten = true;
                }
            }
        }

        //write spanner ending after the last segment, on the last tick
        if (clip || eseg == 0) {
            for (Spanner* s : spanners) {
                if ((s->tick2() == endTick)
                    && !s->isSlur()
                    && (s->track2() == track || (s->track2() == -1 && s->track() == track))
                    && (!clip || s->tick() >= sseg->tick())
                    ) {
                    s->writeSpannerEnd(xml, lastMeasure(), track, endTick);
                }
            }
        }

        if (voiceTagWritten) {
            xml.etag();       // </voice>
        }
    }
}

//---------------------------------------------------------
//   searchTuplet
//    search complete Dom for tuplet id
//    last resort in case of error
//---------------------------------------------------------

Tuplet* Score::searchTuplet(XmlReader& /*e*/, int /*id*/)
{
    return 0;
}
}
