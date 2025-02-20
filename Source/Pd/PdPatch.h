/*
 // Copyright (c) 2015-2022 Pierre Guillot and Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */
#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

#include "PdStorage.h"

extern "C" {
#include "x_libpd_mod_utils.h"
}

namespace pd {

using Connections = std::vector<std::tuple<int, t_object*, int, t_object*>>;
class Instance;

// The Pd patch.
//! @details The class is a wrapper around a Pd patch. The lifetime of the internal patch\n
//! is not guaranteed by the class.
//! @see Instance, Object, Gui
class Patch {
public:
    Patch(void* ptr, Instance* instance, File currentFile = File());

    // The compare equal operator.
    bool operator==(Patch const& other) const
    {
        return getPointer() == other.getPointer();
    }

    void close();

    // Gets the bounds of the patch.
    Rectangle<int> getBounds() const;

    void* createGraph(String const& name, int size, int x, int y);
    void* createGraphOnParent(int x, int y);

    void* createObject(String const& name, int x, int y);
    void removeObject(void* obj);
    void* renameObject(void* obj, String const& name);

    void moveObjects(std::vector<void*> const&, int x, int y);

    void finishRemove();
    void removeSelection();

    void selectObject(void*);
    void deselectAll();

    void setZoom(int zoom);

    void copy();
    void paste();
    void duplicate();

    void startUndoSequence(String name);
    void endUndoSequence(String name);

    void undo();
    void redo();

    enum GroupUndoType {
        Remove = 0,
        Move
    };

    void setCurrent(bool lock = false);

    bool isDirty() const;

    void savePatch(File const& location);
    void savePatch();

    File getCurrentFile() const
    {
        return currentFile;
    }
    void setCurrentFile(File newFile)
    {
        currentFile = newFile;
    }

    bool hasConnection(void* src, int nout, void* sink, int nin);
    bool canConnect(void* src, int nout, void* sink, int nin);
    bool createConnection(void* src, int nout, void* sink, int nin);
    void removeConnection(void* src, int nout, void* sink, int nin);

    Connections getConnections() const;

    t_canvas* getPointer() const
    {
        return static_cast<t_canvas*>(ptr);
    }

    // Gets the objects of the patch.
    std::vector<void*> getObjects();

    String getCanvasContent()
    {
        if (!ptr)
            return {};
        char* buf;
        int bufsize;
        libpd_getcontent(static_cast<t_canvas*>(ptr), &buf, &bufsize);

        auto content = String(buf, static_cast<size_t>(bufsize));
        return content;
    }

    int getIndex(void* obj);

    static t_object* checkObject(void* obj);

    void keyPress(int keycode, int shift);

    String getTitle() const;
    void setTitle(String const& title);

    Instance* instance = nullptr;

private:
    File currentFile;

    void* ptr = nullptr;

    // Initialisation parameters for GUI objects
    // Taken from pd save files, this will make sure that it directly initialises objects with the right parameters
    static inline const std::map<String, String> guiDefaults = {
        { "tgl", "25 0 empty empty empty 17 7 0 10 bgColour fgColour lblColour 0 1" },
        { "hsl", "128 17 0 127 0 0 empty empty empty -2 -8 0 10 bgColour fgColour lblColour 0 1" },
        { "hslider", "128 17 0 127 0 0 empty empty empty -2 -8 0 10 bgColour fgColour lblColour 0 1" },
        { "vsl", "17 128 0 127 0 0 empty empty empty 0 -9 0 10 bgColour fgColour lblColour 0 1" },
        { "vslider", "17 128 0 127 0 0 empty empty empty 0 -9 0 10 bgColour fgColour lblColour 0 1" },
        { "bng", "25 250 50 0 empty empty empty 17 7 0 10 bgColour fgColour lblColour" },
        { "nbx", "4 18 -1e+37 1e+37 0 0 empty empty empty 0 -8 0 10 bgColour lblColour lblColour 0 256" },
        { "hradio", "20 1 0 8 empty empty empty 0 -8 0 10 bgColour fgColour lblColour 0" },
        { "vradio", "20 1 0 8 empty empty empty 0 -8 0 10 bgColour fgColour lblColour 0" },
        { "cnv", "15 100 60 empty empty empty 20 12 0 14 lnColour lblColour" },
        { "vu", "20 120 empty empty -1 -8 0 10 bgColour lblColour 1 0" },
        { "floatatom", "5 0 0 0 empty - - 12" },
        { "listbox", "9 0 0 0 empty - - 0" },
        { "numbox~", "4 16 100 bgColour fgColour 10 0 0 0" },
        { "button", "25 25 bgColour_rgb fgColour_rgb" },
        { "oscope~", "130 130 256 3 128 -1 1 0 0 0 0 fgColour_rgb bgColour_rgb lnColour_rgb 0 empty" },
        { "scope~", "130 130 256 3 128 -1 1 0 0 0 0 fgColour_rgb bgColour_rgb lnColour_rgb 0 empty" },
        { "function", "200 100 empty empty 0 1 bgColour_rgb lblColour_rgb 0 0 0 0 0 1000 0"}
    };
    

    friend class Instance;
    friend class Gui;
    friend class Object;
};
} // namespace pd
