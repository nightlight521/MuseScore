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

#include "fermata.h"
#include "io/xml.h"
#include "score.h"
#include "chordrest.h"
#include "system.h"
#include "measure.h"
#include "staff.h"
#include "stafftype.h"
#include "undo.h"
#include "page.h"
#include "barline.h"
#include "sym.h"

using namespace mu;

namespace Ms {
//---------------------------------------------------------
//   fermataStyle
//---------------------------------------------------------

static const ElementStyle fermataStyle {
    { Sid::fermataPosAbove, Pid::OFFSET },
    { Sid::fermataMinDistance, Pid::MIN_DISTANCE },
};

//---------------------------------------------------------
//   Fermata
//---------------------------------------------------------

Fermata::Fermata(EngravingItem* parent)
    : EngravingItem(ElementType::FERMATA, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF)
{
    setPlacement(Placement::ABOVE);
    _symId         = SymId::noSym;
    _timeStretch   = 1.0;
    setPlay(true);
    initElementStyle(&fermataStyle);
}

Fermata::Fermata(SymId id, EngravingItem* parent)
    : Fermata(parent)
{
    setSymId(id);
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void Fermata::read(XmlReader& e)
{
    while (e.readNextStartElement()) {
        if (!readProperties(e)) {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

bool Fermata::readProperties(XmlReader& e)
{
    const QStringRef& tag(e.name());

    if (tag == "subtype") {
        QString s = e.readElementText();
        SymId id = Sym::name2id(s);
        setSymId(id);
    } else if (tag == "play") {
        setPlay(e.readBool());
    } else if (tag == "timeStretch") {
        _timeStretch = e.readDouble();
    } else if (tag == "offset") {
        if (score()->mscVersion() > 114) {
            EngravingItem::readProperties(e);
        } else {
            e.skipCurrentElement();       // ignore manual layout in older scores
        }
    } else if (EngravingItem::readProperties(e)) {
    } else {
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Fermata::write(XmlWriter& xml) const
{
    if (!xml.canWrite(this)) {
        qDebug("%s not written", name());
        return;
    }
    xml.stag(this);
    xml.tag("subtype", Sym::id2name(_symId));
    writeProperty(xml, Pid::TIME_STRETCH);
    writeProperty(xml, Pid::PLAY);
    writeProperty(xml, Pid::MIN_DISTANCE);
    if (!isStyled(Pid::OFFSET)) {
        writeProperty(xml, Pid::OFFSET);
    }
    EngravingItem::writeProperties(xml);
    xml.etag();
}

//---------------------------------------------------------
//   subtype
//---------------------------------------------------------

int Fermata::subtype() const
{
    QString s = Sym::id2name(_symId);
    if (s.endsWith("Below")) {
        return int(Sym::name2id(s.left(s.size() - 5) + "Above"));
    } else {
        return int(_symId);
    }
}

//---------------------------------------------------------
//   userName
//---------------------------------------------------------

QString Fermata::userName() const
{
    return Sym::id2userName(symId());
}

//---------------------------------------------------------
//   Symbol::draw
//---------------------------------------------------------

void Fermata::draw(mu::draw::Painter* painter) const
{
    TRACE_OBJ_DRAW;
    painter->setPen(curColor());
    drawSymbol(_symId, painter, PointF(-0.5 * width(), 0.0));
}

//---------------------------------------------------------
//   chordRest
//---------------------------------------------------------

ChordRest* Fermata::chordRest() const
{
    if (parent() && parent()->isChordRest()) {
        return toChordRest(parent());
    }
    return 0;
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

Measure* Fermata::measure() const
{
    Segment* s = segment();
    return toMeasure(s ? s->parent() : 0);
}

//---------------------------------------------------------
//   system
//---------------------------------------------------------

System* Fermata::system() const
{
    Measure* m = measure();
    return toSystem(m ? m->parent() : 0);
}

//---------------------------------------------------------
//   page
//---------------------------------------------------------

Page* Fermata::page() const
{
    System* s = system();
    return toPage(s ? s->parent() : 0);
}

//---------------------------------------------------------
//   layout
//    height() and width() should return sensible
//    values when calling this method
//---------------------------------------------------------

void Fermata::layout()
{
    Segment* s = segment();
    setPos(PointF());
    if (!s) {            // for use in palette
        setOffset(0.0, 0.0);
        RectF b(symBbox(_symId));
        setbbox(b.translated(-0.5 * b.width(), 0.0));
        return;
    }

    if (isStyled(Pid::OFFSET)) {
        setOffset(propertyDefault(Pid::OFFSET).value<PointF>());
    }
    EngravingItem* e = s->element(track());
    if (e) {
        if (e->isChord()) {
            rxpos() += score()->noteHeadWidth() * staff()->staffMag(Fraction(0, 1)) * .5;
        } else {
            rxpos() += e->x() + e->width() * staff()->staffMag(Fraction(0, 1)) * .5;
        }
    }

    QString name = Sym::id2name(_symId);
    if (placeAbove()) {
        if (name.endsWith("Below")) {
            _symId = Sym::name2id(name.left(name.size() - 5) + "Above");
        }
    } else {
        rypos() += staff()->height();
        if (name.endsWith("Above")) {
            _symId = Sym::name2id(name.left(name.size() - 5) + "Below");
        }
    }
    RectF b(symBbox(_symId));
    setbbox(b.translated(-0.5 * b.width(), 0.0));
    autoplaceSegmentElement();
}

//---------------------------------------------------------
//   dragAnchorLines
//---------------------------------------------------------

QVector<mu::LineF> Fermata::dragAnchorLines() const
{
    QVector<LineF> result;
    result << LineF(canvasPos(), parentElement()->canvasPos());
    return result;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

QVariant Fermata::getProperty(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::SYMBOL:
        return QVariant::fromValue(_symId);
    case Pid::TIME_STRETCH:
        return timeStretch();
    case Pid::PLAY:
        return play();
    default:
        return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Fermata::setProperty(Pid propertyId, const QVariant& v)
{
    switch (propertyId) {
    case Pid::SYMBOL:
        setSymId(v.value<SymId>());
        break;
    case Pid::PLACEMENT: {
        Placement p = Placement(v.toInt());
        if (p != placement()) {
            QString s = Sym::id2name(_symId);
            bool up = placeAbove();
            if (s.endsWith(up ? "Above" : "Below")) {
                QString s2 = s.left(s.size() - 5) + (up ? "Below" : "Above");
                _symId = Sym::name2id(s2);
            }
            setPlacement(p);
        }
    }
    break;
    case Pid::PLAY:
        setPlay(v.toBool());
        break;
    case Pid::TIME_STRETCH:
        setTimeStretch(v.toDouble());
        score()->fixTicks();
        break;
    default:
        return EngravingItem::setProperty(propertyId, v);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

QVariant Fermata::propertyDefault(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::PLACEMENT:
        return int(track() & 1 ? Placement::BELOW : Placement::ABOVE);
    case Pid::TIME_STRETCH:
        return 1.0;           // articulationList[int(articulationType())].timeStretch;
    case Pid::PLAY:
        return true;
    default:
        break;
    }
    return EngravingItem::propertyDefault(propertyId);
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void Fermata::resetProperty(Pid id)
{
    switch (id) {
    case Pid::TIME_STRETCH:
        setProperty(id, propertyDefault(id));
        return;

    default:
        break;
    }
    EngravingItem::resetProperty(id);
}

//---------------------------------------------------------
//   propertyId
//---------------------------------------------------------

Pid Fermata::propertyId(const QStringRef& xmlName) const
{
    if (xmlName == "subtype") {
        return Pid::SYMBOL;
    }
    return EngravingItem::propertyId(xmlName);
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid Fermata::getPropertyStyle(Pid pid) const
{
    if (pid == Pid::OFFSET) {
        return placeAbove() ? Sid::fermataPosAbove : Sid::fermataPosBelow;
    }
    return EngravingObject::getPropertyStyle(pid);
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

qreal Fermata::mag() const
{
    return staff() ? staff()->staffMag(tick()) * score()->styleD(Sid::articulationMag) : 1.0;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

QString Fermata::accessibleInfo() const
{
    return QString("%1: %2").arg(EngravingItem::accessibleInfo(), userName());
}
}
