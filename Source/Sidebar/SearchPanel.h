/*
 // Copyright (c) 2021-2022 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */


#include <m_pd.h>
#include <m_imp.h>
#include <x_libpd_extra_utils.h>

class SearchPanel : public Component
, public ListBoxModel
, public ScrollBar::Listener
, public KeyListener {
public:
    SearchPanel(PlugDataPluginEditor* pluginEditor) : editor(pluginEditor)
    {
        listBox.setModel(this);
        listBox.setRowHeight(28);
        listBox.setOutlineThickness(0);
        listBox.deselectAllRows();
        
        listBox.getViewport()->setScrollBarsShown(true, false, false, false);
        
        input.setName("sidebar::searcheditor");
        
        input.onTextChange = [this]() {
            updateResults();
        };
        
        input.addKeyListener(this);
        listBox.addKeyListener(this);
        
        closeButton.setName("statusbar:clearsearch");
        closeButton.onClick = [this]() {
            clearSearchTargets();
            input.clear();
            input.giveAwayKeyboardFocus();
            input.repaint();
        };
        
        closeButton.setAlwaysOnTop(true);
        
        addAndMakeVisible(closeButton);
        addAndMakeVisible(listBox);
        addAndMakeVisible(input);
        
        listBox.addMouseListener(this, true);
        
        input.setJustification(Justification::centredLeft);
        input.setBorder({ 1, 23, 3, 1 });
        
        listBox.setColour(ListBox::backgroundColourId, Colours::transparentBlack);
        
        listBox.getViewport()->getVerticalScrollBar().addListener(this);
        
        setWantsKeyboardFocus(false);
    }

    
    void mouseDown(const MouseEvent& e) override{
        MessageManager::callAsync([this](){
            updateSelection();
        });
    }
    
    bool keyPressed (const KeyPress &key) override
    {
        if(key.isKeyCode(KeyPress::returnKey)){
            listBox.keyPressed(KeyPress(KeyPress::downKey));
            return true;
        }
    }
    // Divert up/down key events from text editor to the listbox
    bool keyPressed (const KeyPress &key, Component *originatingComponent) override
    {
        MessageManager::callAsync([this](){
            updateSelection();
        });
        
        auto keyPress = key.isKeyCode(KeyPress::returnKey) ? KeyPress(KeyPress::downKey) : key;
        
        
        // Wrap around when we reach the end of the list
        if(keyPress.isKeyCode(KeyPress::downKey) && listBox.getSelectedRow() == (searchResult.size() - 1)) {
            SparseSet<int> set;
            set.addRange({0, 1});
            listBox.setSelectedRows(set);
            listBox.scrollToEnsureRowIsOnscreen(0);
            return true;
        }
        
        if(keyPress.isKeyCode(KeyPress::upKey) && listBox.getSelectedRow() == 0) {
            int newIdx = searchResult.size() - 1;
            SparseSet<int> set;
            set.addRange({newIdx, newIdx + 1});
            listBox.setSelectedRows(set);
            listBox.scrollToEnsureRowIsOnscreen(newIdx);
            return true;
        }

        
        if(originatingComponent == &listBox && key == keyPress) {
            return listBox.keyPressed(keyPress);
        }
        if(keyPress.isKeyCode(KeyPress::upKey) || keyPress.isKeyCode(KeyPress::downKey)) {
            return listBox.keyPressed(keyPress);;
        }
        
        return false;
    }
    
    void updateSelection() {
        int row = listBox.getSelectedRow();
        if(isPositiveAndBelow(row, searchResult.size())) {
            auto [name, prefix, object, ptr] = searchResult[row];
            
            if(auto* cnv = editor->getCurrentCanvas()) {
                highlightSearchTarget(object);
            }
        }
        
    }
    
    void highlightSearchTarget(Object* target)
    {
        auto* cnv = editor->getCurrentCanvas();
        if(!cnv) return;
        
        for(auto* object : cnv->objects) {
            bool wasSearchTarget = object->isSearchTarget;
            object->isSearchTarget = object == target;
            
            if(wasSearchTarget != object->isSearchTarget) {
                object->repaint();
            }
        }
        
        if(auto* viewport = cnv->viewport) {
            float scale = static_cast<float>(cnv->main.zoomScale.getValue());
            auto pos = target->getBounds().reduced(Object::margin).getCentre() * scale;
            
            pos.x -= viewport->getViewWidth() * 0.5f;
            pos.y -= viewport->getViewHeight() * 0.5f;
            
            viewport->setViewPosition(pos);
        }
    }

    void clearSearchTargets()
    {
        searchResult.clear();
        listBox.updateContent();
        
        for(auto* cnv : editor->canvases) {
            for(auto* object : cnv->objects) {
                bool wasSearchTarget = object->isSearchTarget;
                object->isSearchTarget = false;
                
                if(wasSearchTarget != object->isSearchTarget && cnv->isShowing()) {
                    object->repaint();
                }
            }
        }
    }
    
    void visibilityChanged() override
    {
        if(!isVisible()) {
            clearSearchTargets();
        }
    }
    
    
    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        repaint();
    }
    
    void paint(Graphics& g) override
    {
        g.setColour(findColour(PlugDataColour::sidebarBackgroundColourId));
        g.fillRect(getLocalBounds().withTrimmedBottom(30));
    }
    
    void paintOverChildren(Graphics& g) override
    {
        g.setColour(findColour(PlugDataColour::outlineColourId));
        g.drawLine(0, 29, getWidth(), 29);
        
        g.setFont(getLookAndFeel().getTextButtonFont(closeButton, 30));
        g.setColour(findColour(PlugDataColour::sidebarTextColourId));
        
        g.drawText(Icons::Search, 0, 0, 30, 30, Justification::centred);
        
        if(input.getText().isEmpty()) {
            g.setFont(Font(14));
            g.setColour(findColour(PlugDataColour::sidebarTextColourId).withAlpha(0.5f));
            
            g.drawText("Type to search in patch", 30, 0, 300, 30, Justification::centredLeft);
        }
    }
    
    std::pair<String, String> formatSearchResultString(String name, String prefix, int x, int y)
    {
        
        auto positionString = " (" + String(x) + ", " + String(y) + ")";
        
        int maxWidth = getWidth() - 20;
                
        if(Font().getStringWidth(prefix + name + positionString) > maxWidth) {
            positionString = "";
        }
        
        if(prefix.containsNonWhitespaceChars() && Font().getStringWidth(prefix + name + positionString) > maxWidth) {
            prefix = prefix.upToFirstOccurrenceOf("->", true, true) + " ... " + prefix.fromLastOccurrenceOf("->", true, true);
        }
    
        
        if(prefix.containsNonWhitespaceChars() && Font().getStringWidth(prefix + name + positionString) > maxWidth) {
            prefix = "... -> ";
        }
                
        return {prefix + name, positionString};
    }
    
    void paintListBoxItem(int rowNumber, Graphics& g, int w, int h, bool rowIsSelected) override
    {
        if(!isShowing()) return;
        
        if (rowIsSelected) {
            g.setColour(findColour(PlugDataColour::sidebarActiveBackgroundColourId));
            g.fillRoundedRectangle(4, 2, w - 8, h - 4, Constants::smallCornerRadius);
        }
        
        g.setColour(rowIsSelected ? findColour(PlugDataColour::sidebarActiveTextColourId) : findColour(ComboBox::textColourId));
        
        const auto& [name, prefix, object, ptr] = searchResult[rowNumber];
        
        auto [x, y] = object->getPosition();
        
        auto [text, size] = formatSearchResultString(name, prefix, x, y);
        
        auto positionTextWidth = Font().getStringWidth(size);
        auto positionTextX = getWidth() - positionTextWidth - 16;
        
        g.setFont(Font());
        g.drawText(text, 12, 0, positionTextX - 16, h, Justification::centredLeft, true);
        g.drawText(size, positionTextX, 0, positionTextWidth, h, Justification::centredRight, true);
    }
    
    int getNumRows() override
    {
        return searchResult.size();
    }
    
    Component* refreshComponentForRow(int rowNumber, bool isRowSelected, Component* existingComponentToUpdate) override
    {
        delete existingComponentToUpdate;
        
        return nullptr;
    }
    
    
    void updateResults()
    {
        String query = input.getText();
        
        searchResult.clear();
        auto* cnv = editor->getCurrentCanvas();
        
        if (query.isEmpty() || !cnv) {
            clearSearchTargets();
            return;
        }
        
        searchResult = searchRecursively(cnv, cnv->patch, query);
        
        listBox.updateContent();
        
        if (listBox.getSelectedRow() == -1) {
            listBox.selectRow(0, true, true);
            updateSelection();
        }
    }
    
    void grabFocus() {
        input.grabKeyboardFocus();
    }
    
    
    static Array<std::tuple<String, String, Object*, void*>> searchRecursively(Canvas* topLevelCanvas, pd::Patch& patch, const String& query, Object* topLevelObject = nullptr, String prefix = "") {
        
        auto* instance = patch.instance;
        
        Array<std::tuple<String, String, Object*, void*>> result;
        
        Array<std::pair<void*, Object*>> subpatches;
        
        auto addObject = [&query, &result, &prefix](const String& text, Object* object, void* ptr) {
            
            // Insert in front if the query matches a whole word
            if (text.containsWholeWordIgnoreCase(query)) {
                result.insert(0, {text, prefix, object, ptr});
                return true;
            }
            // Insert in back if it contains the query
            else if (text.containsIgnoreCase(query)) {
                result.add({text, prefix, object, ptr});
                return true;
            }
            
            return false;
        };
        
        for(auto* object : patch.getObjects()) {
            
            Object* topLevel = topLevelObject;
            if(topLevelCanvas) {
                for(auto* obj : topLevelCanvas->objects) {
                    if(obj->getPointer() == object) {
                        topLevel = obj;
                    }
                }
            }
            
            auto className = String::fromUTF8(libpd_get_object_class_name(object));
               
            if(className == "canvas" || className == "graph") {
                // Save them for later, so we can put them at the end of the result
                subpatches.add({object, topLevel});
            }
            else {
                
                bool isGui = !libpd_is_text_object(object);
                
                // If it's a gui add the class name
                if(isGui) {
                    addObject(className, topLevel, object);
                }
                // If it's a text object, message or comment, add the text
                else {
                    char* objectText;
                    int len;
                    libpd_get_object_text(object, &objectText, &len);
                    addObject(String::fromUTF8(objectText, len), topLevel, object);
                }
            }
        }
        
        // Search through subpatches
        for(auto& [object, topLevel] : subpatches)
        {
            auto patch = pd::Patch(object, instance);
            
            char* objectText;
            int len;
            libpd_get_object_text(object, &objectText, &len);
            
            addObject(String::fromUTF8(objectText, len), topLevel, object);
 
            auto objTextStr = String::fromUTF8(objectText, len);
            
            auto tokens = StringArray::fromTokens(objTextStr, false);
            String newPrefix;
            if(tokens[0] == "pd") {
                newPrefix = tokens[0] + " " + tokens[1];
            }
            else {
                newPrefix = tokens[0];
            }
            
            result.addArray(searchRecursively(nullptr, patch, query, topLevel, prefix + newPrefix  + " -> "));
        }
        
        return result;
    }


    void resized() override
    {
        auto tableBounds = getLocalBounds().withTrimmedBottom(30);
        auto inputBounds = tableBounds.removeFromTop(28);
        
        tableBounds.removeFromTop(4);
        
        input.setBounds(inputBounds);
        closeButton.setBounds(inputBounds.removeFromRight(30));
        listBox.setBounds(tableBounds);
    }
    
private:
    ListBox listBox;
    
    Array<std::tuple<String, String, Object*, void*>> searchResult;
    TextEditor input;
    TextButton closeButton = TextButton(Icons::Clear);
    
    PlugDataPluginEditor* editor;
};
