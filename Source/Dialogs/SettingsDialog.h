/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "../Utility/PropertiesPanel.h"

#include "AboutPanel.h"

#include "AudioSettingsPanel.h"
#include "ThemePanel.h"
#include "SearchPathPanel.h"
#include "AdvancedSettingsPanel.h"
#include "KeyMappingPanel.h"
#include "Deken.h"

// Toolbar button for settings panel, with both icon and text
// We have too many specific items to have only icons at this point
struct SettingsToolbarButton : public TextButton
{
    
    String icon;
    String text;
    
    SettingsToolbarButton(String iconToUse, String textToShow) : icon(iconToUse), text(textToShow) {
        
    }
    
    void paint(Graphics& g) override
    {
        
        auto* lnf = dynamic_cast<PlugDataLook*>(&getLookAndFeel());
        if(!lnf) return;
        
        auto b = getLocalBounds().reduced(2);

        g.setColour(findColour(getToggleState() ? PlugDataColour::toolbarActiveColourId : PlugDataColour::toolbarTextColourId));
        
        auto iconBounds = b.removeFromTop(b.getHeight() * 0.65f).withTrimmedTop(5);
        auto textBounds = b.withTrimmedBottom(3);
        
        auto font = lnf->iconFont.withHeight(iconBounds.getHeight() / 1.9f);
        g.setFont (font);
        
        g.drawFittedText(icon, iconBounds, Justification::centred, 1);
        
        font = lnf->defaultFont.withHeight(textBounds.getHeight() / 1.25f);
        g.setFont (font);
        
        // Draw bottom text
        g.drawFittedText(text, textBounds, Justification::centred, 1);
    }
};

struct SettingsDialog : public Component {
    
    SettingsDialog(AudioProcessor& processor, Dialog* dialog, AudioDeviceManager* manager, ValueTree const& settingsTree)
        : audioProcessor(processor)
    {
        setVisible(false);

        toolbarButtons = { new SettingsToolbarButton(Icons::Audio, "Audio"), new SettingsToolbarButton(Icons::Pencil, "Themes"), new SettingsToolbarButton(Icons::Search, "Paths"), new SettingsToolbarButton(Icons::Keyboard, "Shortcuts"), new SettingsToolbarButton(Icons::Externals, "Externals")
#if PLUGDATA_STANDALONE
            , new SettingsToolbarButton(Icons::Wrench, "Advanced")
#endif
        };

        currentPanel = std::clamp(lastPanel.load(), 0, toolbarButtons.size() - 1);

        auto* editor = dynamic_cast<ApplicationCommandManager*>(processor.getActiveEditor());

#if PLUGDATA_STANDALONE
            panels.add(new StandaloneAudioSettings(dynamic_cast<PlugDataAudioProcessor&>(processor), *manager));
#else
            panels.add(new DAWAudioSettings(processor));
#endif

        panels.add(new ThemePanel(settingsTree));
        panels.add(new SearchPathComponent(settingsTree.getChildWithName("Paths")));
        panels.add(new KeyMappingComponent(*editor->getKeyMappings(), settingsTree));
        panels.add(new Deken());
        
#if PLUGDATA_STANDALONE
        panels.add(new AdvancedSettingsPanel(settingsTree));
#endif
        
        for (int i = 0; i < toolbarButtons.size(); i++) {
            toolbarButtons[i]->setClickingTogglesState(true);
            toolbarButtons[i]->setRadioGroupId(0110);
            toolbarButtons[i]->setConnectedEdges(12);
            toolbarButtons[i]->setName("toolbar:settings");
            addAndMakeVisible(toolbarButtons[i]);

            addChildComponent(panels[i]);
            toolbarButtons[i]->onClick = [this, i]() mutable { showPanel(i); };
        }

        toolbarButtons[currentPanel]->setToggleState(true, sendNotification);

        constrainer.setMinimumOnscreenAmounts(600, 400, 400, 400);
    }

    ~SettingsDialog() override
    {
        lastPanel = currentPanel;
        dynamic_cast<PlugDataAudioProcessor*>(&audioProcessor)->saveSettings();
    }

    void resized() override
    {
        auto b = getLocalBounds().withTrimmedTop(toolbarHeight).withTrimmedBottom(6);

        int toolbarPosition = 2;
        for (auto& button : toolbarButtons) {
            button->setBounds(toolbarPosition, 1, 70, toolbarHeight - 2);
            toolbarPosition += 70;
        }
        
        for(auto* panel : panels)
        {
            panel->setBounds(b);
        }
    }

    void paint(Graphics& g) override
    {
        g.setColour(findColour(PlugDataColour::panelBackgroundColourId));
        g.fillRoundedRectangle(getLocalBounds().reduced(1).toFloat(), Constants::windowCornerRadius);

        g.setColour(findColour(PlugDataColour::toolbarBackgroundColourId));

        auto toolbarBounds = Rectangle<float>(1, 1, getWidth() - 2, toolbarHeight);
        g.fillRoundedRectangle(toolbarBounds, Constants::windowCornerRadius);
        g.fillRect(toolbarBounds.withTrimmedTop(15.0f));

#ifdef PLUGDATA_STANDALONE
        bool drawStatusbar = currentPanel > 0;
#else
        bool drawStatusbar = true;
#endif
        
        
        if (drawStatusbar) {
            auto statusbarBounds = getLocalBounds().reduced(1).removeFromBottom(32).toFloat();
            g.setColour(findColour(PlugDataColour::toolbarBackgroundColourId));

            g.fillRect(statusbarBounds.withHeight(20));
            g.fillRoundedRectangle(statusbarBounds, Constants::windowCornerRadius);
        }
        
        g.setColour(findColour(PlugDataColour::outlineColourId));
        g.drawLine(0.0f, toolbarHeight, getWidth(), toolbarHeight);

        if (currentPanel > 0) {
            g.drawLine(0.0f, getHeight() - 33, getWidth(), getHeight() - 33);
        }
    }

    void showPanel(int idx)
    {
        panels[currentPanel]->setVisible(false);
        panels[idx]->setVisible(true);
        currentPanel = idx;
        repaint();
    }

    AudioProcessor& audioProcessor;
    ComponentBoundsConstrainer constrainer;

    static constexpr int toolbarHeight = 55;

    static inline std::atomic<int> lastPanel = 0;
    int currentPanel;
    OwnedArray<Component> panels;
    AudioDeviceManager* deviceManager = nullptr;

    OwnedArray<TextButton> toolbarButtons;
};

struct SettingsPopup : public PopupMenu {
    

    SettingsPopup(AudioProcessor& processor, ValueTree tree) :
    settingsTree(tree),
    themeSelector(tree),
    zoomSelector(tree)
    {
        auto* editor = dynamic_cast<PlugDataPluginEditor*>(processor.getActiveEditor());
        
        addCustomItem(1, themeSelector, 70, 45, false);
        addCustomItem(2, zoomSelector, 70, 30, false);
        addSeparator();
        
        // Toggles hvcc compatibility mode
        bool hvccModeEnabled = settingsTree.hasProperty("HvccMode") ? static_cast<bool>(settingsTree.getProperty("HvccMode")) : false;
        addItem("Compiled mode", true, hvccModeEnabled, [this]() mutable {
            bool ticked = settingsTree.hasProperty("HvccMode") ? static_cast<bool>(settingsTree.getProperty("HvccMode")) : false;
            settingsTree.setProperty("HvccMode", !ticked, nullptr);
        });

        addItem("Compile", [this, editor]() mutable {
            Dialogs::showHeavyExportDialog(&editor->openedDialog, editor);
        });
        

        addSeparator();
        
        bool autoconnectEnabled = settingsTree.hasProperty("AutoConnect") ? static_cast<bool>(settingsTree.getProperty("AutoConnect")) : false;
        
        addItem("Auto-connect objects", true, autoconnectEnabled, [this]() mutable {
            bool ticked = settingsTree.hasProperty("AutoConnect") ? static_cast<bool>(settingsTree.getProperty("AutoConnect")) : false;
            settingsTree.setProperty("AutoConnect", !ticked, nullptr);
        });
        
        addSeparator();
        addItem(5, "Settings");
        addItem(6, "About");
    }
    
    static void showSettingsPopup(AudioProcessor& processor, AudioDeviceManager* manager, Component* centre, ValueTree settingsTree) {
        auto* popup = new SettingsPopup(processor, settingsTree);
        auto* editor = dynamic_cast<PlugDataPluginEditor*>(processor.getActiveEditor());
        
        popup->showMenuAsync(PopupMenu::Options().withMinimumWidth(170).withMaximumNumColumns(1).withTargetComponent(centre).withParentComponent(editor),
            [editor, &processor, popup, manager, centre, settingsTree](int result) {
            
                if (result == 5) {
                    
                    auto* dialog = new Dialog(&editor->openedDialog, editor, 675, 500, editor->getBounds().getCentreY() + 250, true);
                    auto* settingsDialog = new SettingsDialog(processor, dialog, manager, settingsTree);
                    dialog->setViewedComponent(settingsDialog);
                    editor->openedDialog.reset(dialog);
                }
                if (result == 6) {
                    auto* dialog = new Dialog(&editor->openedDialog, editor, 675, 500, editor->getBounds().getCentreY() + 250, true);
                    auto* aboutPanel = new AboutPanel();
                    dialog->setViewedComponent(aboutPanel);
                    editor->openedDialog.reset(dialog);
                }
             
            
            MessageManager::callAsync([popup](){
                delete popup;
            });
            
            });
    }
    
    struct ZoomSelector : public Component
    {
        TextButton zoomIn;
        TextButton zoomOut;
        TextButton zoomReset;
        
        Value zoomValue;
        
        ZoomSelector(ValueTree settingsTree)
        {
            zoomValue = settingsTree.getPropertyAsValue("Zoom", nullptr);
            
            zoomIn.setButtonText("+");
            zoomReset.setButtonText(String(static_cast<float>(zoomValue.getValue()) * 100, 1) + "%");
            zoomOut.setButtonText("-");
            
            addAndMakeVisible(zoomIn);
            addAndMakeVisible(zoomReset);
            addAndMakeVisible(zoomOut);
            
            zoomIn.setConnectedEdges(Button::ConnectedOnLeft);
            zoomOut.setConnectedEdges(Button::ConnectedOnRight);
            zoomReset.setConnectedEdges(12);
            
            zoomIn.onClick = [this](){
                applyZoom(true);
            };
            zoomOut.onClick = [this](){
                applyZoom(false);
            };
            zoomReset.onClick = [this](){
                resetZoom();
            };
        }
        
        void applyZoom(bool zoomIn)
        {
            float value = static_cast<float>(zoomValue.getValue());

            // Apply limits
            value = std::clamp(zoomIn ? value + 0.1f : value - 0.1f, 0.5f, 2.0f);

            // Round in case we zoomed with scrolling
            value = static_cast<float>(static_cast<int>(round(value * 10.))) / 10.;

            zoomValue = value;

            zoomReset.setButtonText(String(value * 100.0f, 1) + "%");
        }
        
        void resetZoom() {
            zoomValue = 1.0f;
            zoomReset.setButtonText("100.0%");
        }
        
        void resized() override
        {
            auto bounds = getLocalBounds().reduced(8, 4);
            int buttonWidth = (getWidth() - 8) / 3;
            
            zoomOut.setBounds(bounds.removeFromLeft(buttonWidth).expanded(1, 0));
            zoomReset.setBounds(bounds.removeFromLeft(buttonWidth).expanded(1, 0));
            zoomIn.setBounds(bounds.removeFromLeft(buttonWidth).expanded(1, 0));
        }
    };

    struct ThemeSelector : public Component
    {
        ThemeSelector(ValueTree settingsTree) {
            theme.referTo(settingsTree.getPropertyAsValue("Theme", nullptr));
        }
        
        void paint(Graphics& g)
        {
            auto firstBounds = getLocalBounds();
            auto secondBounds = firstBounds.removeFromLeft(getWidth() / 2.0f);
            
            firstBounds = firstBounds.withSizeKeepingCentre(30, 30);
            secondBounds = secondBounds.withSizeKeepingCentre(30, 30);
            
            g.setColour(Colour(25, 25, 25));
            g.fillEllipse(firstBounds.toFloat());
            
            g.setColour(Colour(240, 240, 240));
            g.fillEllipse(secondBounds.toFloat());

            g.setColour(findColour(PlugDataColour::outlineColourId));
            g.drawEllipse(firstBounds.toFloat(), 1.0f);
            g.drawEllipse(secondBounds.toFloat(), 1.0f);
            
            auto tick = getLookAndFeel().getTickShape(0.6f);
            auto tickBounds = Rectangle<int>();
            
            if(!static_cast<bool>(theme.getValue())) {
                g.setColour(Colour(240, 240, 240));
                tickBounds = firstBounds;
            }
            else {
                g.setColour(Colour(25, 25, 25));
                tickBounds = secondBounds;
            }
            
            g.fillPath (tick, tick.getTransformToScaleToFit (tickBounds.reduced (9, 9).toFloat(), false));

        }
        
        void mouseUp(const MouseEvent& e)
        {
            auto firstBounds = getLocalBounds();
            auto secondBounds = firstBounds.removeFromLeft(getWidth() / 2.0f);
            
            firstBounds = firstBounds.withSizeKeepingCentre(30, 30);
            secondBounds = secondBounds.withSizeKeepingCentre(30, 30);
            
            if(firstBounds.contains(e.x, e.y)) {
                theme = false;
                repaint();
            }
            else if(secondBounds.contains(e.x, e.y)) {
                theme = true;
                repaint();
            }
        }
        
        Value theme;
    };
    
    ThemeSelector themeSelector;
    ZoomSelector zoomSelector;

    
    ValueTree settingsTree;
};
