/*
  Q Light Controller Plus
  qlcpalette.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scenevalue.h"
#include "doc.h"

#define KXMLQLCPaletteType  "Type"
#define KXMLQLCPaletteName  "Name"
#define KXMLQLCPaletteValue "Value"

QLCPalette::QLCPalette(QLCPalette::PaletteType type, QObject *parent)
    : QObject(parent)
    , m_id(QLCPalette::invalidId())
    , m_type(type)
{

}

QLCPalette::~QLCPalette()
{

}

/************************************************************************
 * Properties
 ************************************************************************/

quint32 QLCPalette::id() const
{
    return m_id;
}

void QLCPalette::setID(quint32 id)
{
    m_id = id;
}

quint32 QLCPalette::invalidId()
{
    return UINT_MAX;
}

QLCPalette::PaletteType QLCPalette::type() const
{
    return m_type;
}

QString QLCPalette::typeToString(QLCPalette::PaletteType type)
{
    switch (type)
    {
        case Dimmer:    return "Dimmer";
        case Color:     return "Color";
        case Position:  return "Position";
        case Shutter:   return "Shutter";
        case Gobo:      return "Gobo";
        case Undefined: return "";
    }

    return "";
}

QLCPalette::PaletteType QLCPalette::stringToType(const QString &str)
{
    if (str == "Dimmer")
        return Dimmer;
    else if (str == "Color")
        return Color;
    else if (str == "Position")
        return Position;
    else if (str == "Shutter")
        return Shutter;
    else if (str == "Gobo")
        return Gobo;

    return Undefined;
}

QString QLCPalette::iconResource(bool svg) const
{
    QString prefix = svg ? "qrc" : "";
    QString ext = svg ? "svg" : "png";

    switch(type())
    {
        case Dimmer: return QString("%1:/intensity.%2").arg(prefix).arg(ext);
        case Color: return QString("%1:/color.%2").arg(prefix).arg(ext);
        case Position: return QString("%1:/position.%2").arg(prefix).arg(ext);
        case Shutter: return QString("%1:/shutter.%2").arg(prefix).arg(ext);
        case Gobo: return QString("%1:/gobo.%2").arg(prefix).arg(ext);
        default: return "";
    }
}

QString QLCPalette::name() const
{
    return m_name;
}

void QLCPalette::setName(const QString &name)
{
    if (name == m_name)
        return;

    m_name = QString(name);
    emit nameChanged();
}

QVariant QLCPalette::value() const
{
    if (m_values.isEmpty())
        return QVariant();

    return m_values.first();
}

void QLCPalette::setValue(QVariant val)
{
    m_values.clear();
    m_values.append(val);
}

void QLCPalette::setValue(QVariant val1, QVariant val2)
{
    m_values.clear();
    m_values.append(val1);
    m_values.append(val2);
}

QList<SceneValue> QLCPalette::valuesFromFixtures(Doc *doc, QList<quint32> fixtures)
{
    QList<SceneValue> list;

    foreach (quint32 id, fixtures)
    {
        Fixture *fixture = doc->fixture(id);
        if (fixture == NULL)
            continue;

        switch(type())
        {
            case Dimmer:
            {
                quint32 intCh = fixture->masterIntensityChannel();
                if (intCh != QLCChannel::invalid())
                    list << SceneValue(id, intCh, uchar(value().toUInt()));
            }
            break;
            case Color:
            {
                // TODO loop through heads
                QColor col = value().value<QColor>();
                QVector<quint32> rgbCh = fixture->rgbChannels();
                if (rgbCh.size() == 3)
                {
                    list << SceneValue(id, rgbCh.at(0), uchar(col.red()));
                    list << SceneValue(id, rgbCh.at(1), uchar(col.green()));
                    list << SceneValue(id, rgbCh.at(2), uchar(col.blue()));
                    break;
                }
                QVector<quint32> cmyCh = fixture->cmyChannels();
                if (cmyCh.size() == 3)
                {
                    list << SceneValue(id, cmyCh.at(0), uchar(col.cyan()));
                    list << SceneValue(id, cmyCh.at(1), uchar(col.magenta()));
                    list << SceneValue(id, cmyCh.at(2), uchar(col.yellow()));
                }
            }
            break;
            case Position:
            {
                if (m_values.count() == 2)
                {
                    list << fixture->positionToValues(QLCChannel::Pan, m_values.at(0).toInt());
                    list << fixture->positionToValues(QLCChannel::Tilt, m_values.at(1).toInt());
                }
            }
            break;
            case Shutter:
            {
                quint32 shCh = fixture->channelNumber(QLCChannel::Shutter, QLCChannel::MSB);
                if (shCh != QLCChannel::invalid())
                    list << SceneValue(id, shCh, uchar(value().toUInt()));
            }
            break;
            case Gobo:
            {
                quint32 goboCh = fixture->channelNumber(QLCChannel::Gobo, QLCChannel::MSB);
                if (goboCh != QLCChannel::invalid())
                    list << SceneValue(id, goboCh, uchar(value().toUInt()));
            }
            break;
            case Undefined:
            break;
        }
    }

    return list;
}

QList<SceneValue> QLCPalette::valuesFromFixtureGroups(Doc *doc, QList<quint32> groups)
{
    QList<SceneValue> list;

    foreach (quint32 id, groups)
    {
        FixtureGroup *group = doc->fixtureGroup(id);
        if (group == NULL)
            continue;

        list << valuesFromFixtures(doc, group->fixtureList());
    }

    return list;
}

/************************************************************************
 * Load & Save
 ************************************************************************/

bool QLCPalette::loader(QXmlStreamReader &xmlDoc, Doc *doc)
{
    QLCPalette *palette = new QLCPalette(Dimmer, doc);
    Q_ASSERT(palette != NULL);

    if (palette->loadXML(xmlDoc) == true)
    {
        doc->addPalette(palette, palette->id());
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "QLCPalette" << palette->name() << "cannot be loaded.";
        delete palette;
        return false;
    }

    return true;
}

bool QLCPalette::loadXML(QXmlStreamReader &doc)
{
    if (doc.name() != KXMLQLCPalette)
    {
        qWarning() << Q_FUNC_INFO << "Palette node not found";
        return false;
    }

    QXmlStreamAttributes attrs = doc.attributes();

    bool ok = false;
    quint32 id = attrs.value(KXMLQLCPaletteID).toString().toUInt(&ok);
    if (ok == false)
    {
        qWarning() << "Invalid Palette ID:" << attrs.value(KXMLQLCPaletteID).toString();
        return false;
    }

    m_id = id;

    if (attrs.hasAttribute(KXMLQLCPaletteType) == false)
    {
        qWarning() << "Palette type not found!";
        return false;
    }

    m_type = stringToType(attrs.value(KXMLQLCPaletteType).toString());

    if (attrs.hasAttribute(KXMLQLCPaletteName))
        m_name = attrs.value(KXMLQLCPaletteName).toString();

    if (attrs.hasAttribute(KXMLQLCPaletteValue))
    {
        QString strVal = attrs.value(KXMLQLCPaletteValue).toString();
        switch (m_type)
        {
            case Dimmer:
                setValue(strVal.toInt());
            break;
            case Color:
                setValue(QColor(strVal));
            break;
            case Position:
            {
                QStringList posList = strVal.split(",");
                if (posList.count() == 2)
                    setValue(posList.at(0).toInt(), posList.at(1).toInt());
            }
            break;
            case Shutter:   break;
            case Gobo:      break;
            case Undefined: break;
        }
    }

    return true;
}

bool QLCPalette::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    /* write a Palette entry */
    doc->writeStartElement(KXMLQLCPalette);
    doc->writeAttribute(KXMLQLCPaletteID, QString::number(this->id()));
    doc->writeAttribute(KXMLQLCPaletteType, typeToString(m_type));
    doc->writeAttribute(KXMLQLCPaletteName, this->name());

    switch (m_type)
    {
        case Dimmer:
            doc->writeAttribute(KXMLQLCPaletteValue, value().toString());
        break;
        case Color:
        {
            QColor col = value().value<QColor>();
            doc->writeAttribute(KXMLQLCPaletteValue, col.name());
        }
        break;
        case Position:
            doc->writeAttribute(KXMLQLCPaletteValue,
                                QString("%1,%2").arg(m_values.at(0).toInt()).arg(m_values.at(1).toInt()));
        break;
        case Shutter:   break;
        case Gobo:      break;
        case Undefined: break;
    }

    doc->writeEndElement();

    return true;
}
