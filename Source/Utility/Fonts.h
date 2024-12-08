#pragma once
#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Utility/Config.h"

enum FontStyle {
    Regular,
    Bold,
    Semibold,
    Thin,
    Monospace,
    Variable,
    Tabular
};

struct Fonts {
    Fonts()
    {
        Typeface::setTypefaceCacheSize(7);

        // Our unicode font is too big, the compiler will run out of memory
        // To prevent this, we split the BinaryData into multiple files, and add them back together here
        HeapArray<char> interUnicode;
        interUnicode.reserve(17 * 1024 * 1024); // Reserve 17mb to prevent more allocations
        int i = 0;
        while (true) {
            int size;
            auto* resource = BinaryData::getNamedResource((String("InterUnicode_") + String(i) + "_ttf").toRawUTF8(), size);

            if (!resource) {
                break;
            }

            interUnicode.insert(interUnicode.end(), resource, resource + size);
            i++;
        }

        // Initialise typefaces
        defaultFont = Typeface::createSystemTypefaceFor(interUnicode.data(), interUnicode.size());
        currentFont = defaultFont;

        for (auto font : fontRegistry) {
            if (font.second.loadInJUCE)
                typefaces[font.first] =  Typeface::createSystemTypefaceFor(fontRegistry.at(font.first).data, fontRegistry.at(font.first).size);

        }
        instance = this;
    }

    static Font getCurrentFont() { return Font(instance->currentFont); }
    static Font getDefaultFont() { return Font(instance->defaultFont); }

    static Font getThinFont()           { return Font(instance->typefaces["Inter-Thin"]); }
    static Font getBoldFont()           { return Font(instance->typefaces["Inter-Bold"]); }
    static Font getSemiBoldFont()       { return Font(instance->typefaces["Inter-SemiBold"]); }
    static Font getTabularNumbersFont() { return Font(instance->typefaces["Inter-Tabular"]); }
    static Font getVariableFont()       { return Font(instance->typefaces["Inter-Variable"]); }
    static Font getIconFont()           { return Font(instance->typefaces["icon_font-Regular"]); }
    static Font getMonospaceFont()      { return Font(instance->typefaces["Mono"]); }

    static Font setCurrentFont(Font const& font) { return instance->currentFont = font.getTypefacePtr(); }

    static Array<File> getFontsInFolder(File const& patchFile)
    {
        return patchFile.getParentDirectory().findChildFiles(File::findFiles, true, "*.ttf;*.otf;");
    }

    static std::optional<Font> findFont(File const& dirToSearch, String const& typefaceFileName)
    {
        Array<File> fontFiles = dirToSearch.getParentDirectory().findChildFiles(File::findFiles, true, "*.ttf;*.otf;");
        
        for (auto font : fontFiles) {
            if(font.getFileNameWithoutExtension() == typefaceFileName)
            {
                auto it = fontTable.find(font.getFullPathName());
                if(it != fontTable.end())
                {
                    return it->second;
                }
                else if (font.existsAsFile())
                {
                    auto fileStream = font.createInputStream();
                    if (fileStream == nullptr) break;

                   MemoryBlock fontData;
                   fileStream->readIntoMemoryBlock(fontData);
                   auto typeface = Typeface::createSystemTypefaceFor(fontData.getData(), fontData.getSize());
                   fontTable[font.getFullPathName()] = typeface;
                   return typeface;
                }
            }
        }
        
        return std::nullopt;
    }

    // For drawing icons with icon font
    static void drawIcon(Graphics& g, String const& icon, Rectangle<int> bounds, Colour colour, int fontHeight = -1, bool centred = true)
    {
        if (fontHeight < 0)
            fontHeight = bounds.getHeight() / 1.2f;

        auto justification = centred ? Justification::centred : Justification::centredLeft;
        auto font = Fonts::getIconFont().withHeight(fontHeight);
        g.setFont(font);
        g.setColour(colour);
        g.drawText(icon, bounds, justification, false);
    }

    static void drawIcon(Graphics& g, String const& icon, int x, int y, int size, Colour colour, int fontHeight = -1, bool centred = true)
    {
        drawIcon(g, icon, { x, y, size, size }, colour, fontHeight, centred);
    }

    static Font getFontFromStyle(FontStyle style)
    {
        Font font;
        switch (style) {
        case Regular:
            font = Fonts::getCurrentFont();
            break;
        case Bold:
            font = Fonts::getBoldFont();
            break;
        case Semibold:
            font = Fonts::getSemiBoldFont();
            break;
        case Thin:
            font = Fonts::getThinFont();
            break;
        case Monospace:
            font = Fonts::getMonospaceFont();
            break;
        case Variable:
            font = Fonts::getVariableFont();
            break;
        case Tabular:
            font = Fonts::getTabularNumbersFont();
            break;
        }
        return font;
    }

    // For drawing bold, semibold or thin text
    static void drawStyledTextSetup(Graphics& g, Colour colour, FontStyle style, int fontHeight = 15)
    {
        g.setFont(getFontFromStyle(style).withHeight(fontHeight));
        g.setColour(colour);
    }

    // rectangle float version
    static void drawStyledText(Graphics& g, String const& textToDraw, Rectangle<float> bounds, Colour colour, FontStyle style, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        drawStyledTextSetup(g, colour, style, fontHeight);
        g.drawText(textToDraw, bounds, justification);
    }

    // rectangle int version
    static void drawStyledText(Graphics& g, String const& textToDraw, Rectangle<int> bounds, Colour colour, FontStyle style, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        drawStyledTextSetup(g, colour, style, fontHeight);
        g.drawText(textToDraw, bounds, justification);
    }

    // int version
    static void drawStyledText(Graphics& g, String const& textToDraw, int x, int y, int w, int h, Colour colour, FontStyle style, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        drawStyledTextSetup(g, colour, style, fontHeight);
        g.drawText(textToDraw, Rectangle<int>(x, y, w, h), justification);
    }

    // For drawing regular text
    static void drawText(Graphics& g, String const& textToDraw, Rectangle<float> bounds, Colour colour, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        g.setFont(Fonts::getCurrentFont().withHeight(fontHeight));
        g.setColour(colour);
        g.drawText(textToDraw, bounds, justification);
    }

    // For drawing regular text
    static void drawText(Graphics& g, String const& textToDraw, Rectangle<int> bounds, Colour colour, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        g.setFont(Fonts::getCurrentFont().withHeight(fontHeight));
        g.setColour(colour);
        g.drawText(textToDraw, bounds, justification);
    }

    static void drawText(Graphics& g, String const& textToDraw, int x, int y, int w, int h, Colour colour, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        drawText(g, textToDraw, Rectangle<int>(x, y, w, h), colour, fontHeight, justification);
    }

    static void drawFittedText(Graphics& g, String const& textToDraw, Rectangle<int> bounds, Colour colour, int numLines = 1, float minimumHoriontalScale = 1.0f, float fontHeight = 15.0f, Justification justification = Justification::centredLeft, FontStyle style = FontStyle::Regular)
    {
        g.setFont(getFontFromStyle(style).withHeight(fontHeight));
        g.setColour(colour);
        g.drawFittedText(textToDraw, bounds, justification, numLines, minimumHoriontalScale);
    }

    static void drawFittedText(Graphics& g, String const& textToDraw, int x, int y, int w, int h, Colour const& colour, int numLines = 1, float minimumHoriontalScale = 1.0f, int fontHeight = 15, Justification justification = Justification::centredLeft)
    {
        drawFittedText(g, textToDraw, { x, y, w, h }, colour, numLines, minimumHoriontalScale, fontHeight, justification);
    }

    struct FontResource
    {
        const void* data;
        size_t size;
        bool loadInNVG;
        bool loadInJUCE;
    };

    // Centralized map for font registry
    static inline const std::map<std::string, FontResource> fontRegistry = {
        { "Inter",              { BinaryData::InterRegular_ttf,       BinaryData::InterRegular_ttfSize,       true,  false } }, // macOS uses Inter
        { "Inter-Regular",      { BinaryData::InterRegular_ttf,       BinaryData::InterRegular_ttfSize,       true,  false } }, // windows uses Inter-Regular Oof!
        { "Inter-Thin",         { BinaryData::InterThin_ttf,          BinaryData::InterThin_ttfSize,          false, true } },
        { "Inter-Bold",         { BinaryData::InterBold_ttf,          BinaryData::InterBold_ttfSize,          true,  true } },
        { "Inter-SemiBold",     { BinaryData::InterSemiBold_ttf,      BinaryData::InterSemiBold_ttfSize,      true,  true } },
        { "Inter-Tabular",      { BinaryData::InterTabular_ttf,       BinaryData::InterTabular_ttfSize,       true,  true } },
        { "Inter-Variable",     { BinaryData::InterVariable_ttf,      BinaryData::InterVariable_ttfSize,      false, true } },
        { "icon_font-Regular",  { BinaryData::IconFont_ttf,           BinaryData::IconFont_ttfSize,           true,  true } },
        { "Mono",               { BinaryData::RobotoMono_Regular_ttf, BinaryData::RobotoMono_Regular_ttfSize, false, true } }
    };

private:
    // This is effectively a singleton because it's loaded through SharedResourcePointer
    static inline Fonts* instance = nullptr;

    // Default typeface is Inter combined with Unicode symbols from GoNotoUniversal and emojis from NotoEmoji
    std::unordered_map<String, Typeface::Ptr> typefaces;

    Typeface::Ptr currentFont;
    Typeface::Ptr defaultFont;
    
    static inline UnorderedMap<String, Font> fontTable = UnorderedMap<String, Font>();
};
