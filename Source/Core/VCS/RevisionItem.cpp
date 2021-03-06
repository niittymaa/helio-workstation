/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common.h"
#include "RevisionItem.h"
#include "DiffLogic.h"

using namespace VCS;

RevisionItem::RevisionItem(Pack::Ptr packPtr, Type type, TrackedItem *targetToCopy) :
    vcsItemType(type),
    pack(packPtr)
{
    if (targetToCopy != nullptr)
    {
        this->description = targetToCopy->getVCSName();
        this->vcsUuid = targetToCopy->getUuid();

        this->logic = DiffLogic::createLogicCopy(*targetToCopy, *this);

        // окей, здесь тупо скачать себе дельты
        // на вход нам передают готовый дифф
        // пустой, если тип - "удалено"

        // итак, все дельты диффа лучше всего держать в памяти или временном файле.
        // в пак они помещаются после коммита.
        // и, да, получится, что сериализация слоев вызывается из другого потока.

        for (int i = 0; i < targetToCopy->getNumDeltas(); ++i)
        {
            this->deltas.add(new Delta(*targetToCopy->getDelta(i)));
            this->deltasData.add(targetToCopy->createDeltaDataFor(i));
        }
    }
}

RevisionItem::~RevisionItem() {}



void RevisionItem::flushData()
{
    for (int i = 0; i < this->deltasData.size(); ++i)
    {
        this->pack->setDeltaDataFor(this->getUuid(), this->deltas[i]->getUuid(), *this->deltasData[i]);
    }

    this->deltasData.clear();
}

Pack::Ptr RevisionItem::getPackPtr() const
{
    return this->pack;
}

RevisionItem::Type RevisionItem::getType() const
{
    return this->vcsItemType;
}

String RevisionItem::getTypeAsString() const
{
    if (this->vcsItemType == Added)
    {
        return TRANS("vcs::delta::type::added");
    }
    if (this->vcsItemType == Removed)
    {
        return TRANS("vcs::delta::type::removed");
    }
    else if (this->vcsItemType == Changed)
    {
        return TRANS("vcs::delta::type::changed");
    }

    return "";
}

void RevisionItem::importDataForDelta(const XmlElement *deltaDataToCopy, const String &deltaUuid)
{
    for (int i = 0; i < this->deltas.size(); ++i)
    {
        const Delta *delta = this->deltas[i];
        
        if (delta->getUuid().toString() == deltaUuid)
        {
            while (this->deltasData.size() <= i)
            {
                this->deltasData.add(new XmlElement("dummy"));
            }
            
            auto deepCopy = new XmlElement(*deltaDataToCopy);
            this->deltasData.set(i, deepCopy);
            break;
        }
    }
}


//===----------------------------------------------------------------------===//
// TrackedItem
//===----------------------------------------------------------------------===//

int RevisionItem::getNumDeltas() const
{
    return this->deltas.size();
}

Delta *RevisionItem::getDelta(int index) const
{
    return this->deltas[index];
}

XmlElement *RevisionItem::createDeltaDataFor(int index) const
{
    // ситуация, когда мы представляем незакоммиченные изменения, и *все* сериализованные данные у нас есть
    if (index < this->deltasData.size())
    {
        return new XmlElement(*this->deltasData[index]);
    }

    // иначе мы представляем собой тупо запись в дереве истории,
    // и все, что у нас есть - это уиды.
    // надо как-то обратиться к паку
    // и запросить эти данные по паре item id : delta id

    return this->pack->createDeltaDataFor(this->getUuid(), this->deltas[index]->getUuid());
}

String RevisionItem::getVCSName() const
{
    return this->description;
}

DiffLogic *VCS::RevisionItem::getDiffLogic() const
{
    return this->logic;
}


//===----------------------------------------------------------------------===//
// Serializable
//===----------------------------------------------------------------------===//

XmlElement *RevisionItem::serialize() const
{
    auto const xml = new XmlElement(Serialization::VCS::revisionItem);

    this->serializeVCSUuid(*xml);

    xml->setAttribute(Serialization::VCS::revisionItemType, this->getType());
    xml->setAttribute(Serialization::VCS::revisionItemName, this->getVCSName());
    xml->setAttribute(Serialization::VCS::revisionItemDiffLogic, this->getDiffLogic()->getType());

    for (auto delta : this->deltas)
    {
        xml->prependChildElement(delta->serialize());
    }

    return xml;
}

void RevisionItem::deserialize(const XmlElement &xml)
{
    this->reset();

    const XmlElement *root = xml.hasTagName(Serialization::VCS::revisionItem) ?
                             &xml : xml.getChildByName(Serialization::VCS::revisionItem);

    if (root == nullptr) { return; }

    this->deserializeVCSUuid(*root);

    this->description = root->getStringAttribute(Serialization::VCS::revisionItemName, "");

    const int type = root->getIntAttribute(Serialization::VCS::revisionItemType, Undefined);
    this->vcsItemType = static_cast<Type>(type);

    const String logicType =
        root->getStringAttribute(Serialization::VCS::revisionItemDiffLogic,
                                 "");

    jassert(logicType != "");

    this->logic = DiffLogic::createLogicFor(*this, logicType);

    forEachXmlChildElement(*root, e)
    {
        Delta *delta(new Delta(DeltaDescription(""), ""));
        delta->deserialize(*e);
        this->deltas.add(delta);
    }
}

void RevisionItem::reset()
{
    this->deltas.clear();
    this->description = "";
    this->vcsItemType = Undefined;
}
