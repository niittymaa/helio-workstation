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

#pragma once

//[Headers]
#include "HighlightedComponent.h"
#include "TreeItem.h"

class IconComponent;
//[/Headers]


class HeadlineDropdown  : public Component,
                          private Timer
{
public:

    HeadlineDropdown (WeakReference<TreeItem> targetItem);

    ~HeadlineDropdown();

    //[UserMethods]
    void childBoundsChanged(Component *) override;
    //[/UserMethods]

    void paint (Graphics& g) override;
    void resized() override;
    void mouseExit (const MouseEvent& e) override;
    void mouseDown (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;
    void inputAttemptWhenModal() override;


private:

    //[UserVariables]

    WeakReference<TreeItem> item;

    void timerCallback() override;
    void syncWidthWithContent();

    //[/UserVariables]

    ScopedPointer<Label> titleLabel;
    ScopedPointer<IconComponent> icon;
    ScopedPointer<Component> content;
    Path internalPath1;
    Path internalPath2;
    Path internalPath3;
    Path internalPath4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeadlineDropdown)
};
