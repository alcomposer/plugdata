/*
 // Copyright (c) 2021-2022 Timothy Schoen and Alex Mitchell
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

using namespace gl;

#include <nanovg.h>

#include <utility>

#include "Object.h"
#include "Objects/ObjectBase.h"
#include "Connection.h"
#include "Canvas.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include "Utility/SettingsFile.h"

// Special viewport that shows scrollbars on top of content instead of next to it
class CanvasViewport : public Viewport
    , public NVGComponent
    , public MultiTimer {
    class MousePanner : public MouseListener {
    public:
        explicit MousePanner(CanvasViewport* vp)
            : viewport(vp)
        {
        }

        void enablePanning(bool enabled, Canvas* canvas = nullptr)
        {
            auto attachMouseListnerToCanvas = [this, enabled](Component* component) -> bool {
                if (component) {
                    if (enabled) {
                        component->addMouseListener(this, false);
                    } else {
                        component->removeMouseListener(this);
                    }
                    return true;
                }
                return false;
            };

            if (attachMouseListnerToCanvas(canvas))
                return;

            attachMouseListnerToCanvas(viewport->getViewedComponent());
        }

        // warning: this only works because Canvas::mouseDown gets called before the listener's mouse down
        // thus giving us a chance to attach the mouselistener on the middle-mouse click event
        // Specifically, we use the hitTest in on-canvas objects to check if panning mod is down
        // Because the hitTest can decided where the mouseEvent goes.
        void mouseDown(MouseEvent const& e) override
        {
            if (!e.mods.isLeftButtonDown() && !e.mods.isMiddleButtonDown())
                return;

            // Cancel the animation timer for the search panel
            viewport->stopTimer(Timers::AnimationTimer);

            e.originalComponent->setMouseCursor(MouseCursor::DraggingHandCursor);
            downPosition = viewport->getViewPosition();
            downCanvasOrigin = viewport->cnv->canvasOrigin;
        }

        void mouseDrag(MouseEvent const& e) override
        {
            float scale = std::sqrt(std::abs(viewport->cnv->getTransform().getDeterminant()));

            auto infiniteCanvasOriginOffset = (viewport->cnv->canvasOrigin - downCanvasOrigin) * scale;
            viewport->setViewPosition(infiniteCanvasOriginOffset + downPosition - (scale * e.getOffsetFromDragStart().toFloat()).roundToInt());
        }

    private:
        CanvasViewport* viewport;
        Point<int> downPosition;
        Point<int> downCanvasOrigin;
    };

    class ViewportScrollBar : public Component {
        struct FadeTimer : private ::Timer {
            std::function<bool()> callback;

            void start(int interval, std::function<bool()> cb)
            {
                callback = std::move(cb);
                startTimer(interval);
            }

            void timerCallback() override
            {
                if (callback())
                    stopTimer();
            }
        };

        struct FadeAnimator : private ::Timer {
            explicit FadeAnimator(ViewportScrollBar* target)
                : targetComponent(target)
            {
            }

            void timerCallback() override
            {
                auto growth = targetComponent->growAnimation;
                if (growth < growthTarget) {
                    growth += 0.1f;
                    if (growth >= growthTarget) {
                        stopTimer();
                    }
                    targetComponent->setGrowAnimation(growth);
                } else if (growth > growthTarget) {
                    growth -= 0.1f;
                    if (growth <= growthTarget) {
                        growth = growthTarget;
                        stopTimer();
                    }
                    targetComponent->setGrowAnimation(growth);
                } else {
                    stopTimer();
                }
            }

            void grow()
            {
                growthTarget = 0.0f;
                startTimerHz(60);
            }

            void shrink()
            {
                growthTarget = 1.0f;
                startTimerHz(60);
            }

            ViewportScrollBar* targetComponent;
            float growthTarget = 0.0f;
        };

    public:
        ViewportScrollBar(bool isVertical, CanvasViewport* viewport)
            : isVertical(isVertical)
            , viewport(viewport)
        {
            scrollBarThickness = viewport->getScrollBarThickness();
        }

        bool hitTest(int x, int y) override
        {
            Rectangle<float> fullBounds;
            if (isVertical)
                fullBounds = thumbBounds.withY(2).withHeight(getHeight() - 4);
            else
                fullBounds = thumbBounds.withX(2).withWidth(getWidth() - 4);

            if (fullBounds.contains(x, y))
                return true;

            return false;
        }

        void mouseDrag(MouseEvent const& e) override
        {
            Point<float> delta;
            if (isVertical) {
                delta = Point<float>(0, e.getDistanceFromDragStartY());
            } else {
                delta = Point<float>(e.getDistanceFromDragStartX(), 0);
            }
            viewport->setViewPosition(viewPosition + (delta * 4).toInt());
            repaint();
        }

        void setGrowAnimation(float newGrowValue)
        {
            growAnimation = newGrowValue;
            repaint();
        }

        void mouseDown(MouseEvent const& e) override
        {
            if (!e.mods.isLeftButtonDown())
                return;

            isMouseDragging = true;
            viewPosition = viewport->getViewPosition();
            repaint();
        }

        void mouseUp(MouseEvent const& e) override
        {
            isMouseDragging = false;
            if (e.mouseWasDraggedSinceMouseDown()) {
                if (!isMouseOver)
                    animator.shrink();
            }
            repaint();
        }

        void mouseEnter(MouseEvent const& e) override
        {
            isMouseOver = true;
            animator.grow();
            repaint();
        }

        void mouseExit(MouseEvent const& e) override
        {
            isMouseOver = false;
            if (!isMouseDragging)
                animator.shrink();
            repaint();
        }

        void setRangeLimitsAndCurrentRange(float minTotal, float maxTotal, float minCurrent, float maxCurrent)
        {
            totalRange = { minTotal, maxTotal };
            currentRange = { minCurrent, maxCurrent };
            updateThumbBounds();
        }

        void updateThumbBounds()
        {
            auto thumbStart = jmap<int>(currentRange.getStart(), totalRange.getStart(), totalRange.getEnd(), 0, isVertical ? getHeight() : getWidth());
            auto thumbEnd = jmap<int>(currentRange.getEnd(), totalRange.getStart(), totalRange.getEnd(), 0, isVertical ? getHeight() : getWidth());

            if (isVertical)
                thumbBounds = Rectangle<float>(0, thumbStart, getWidth(), thumbEnd - thumbStart);
            else
                thumbBounds = Rectangle<float>(thumbStart, 0, thumbEnd - thumbStart, getHeight());
            repaint();
        }

        void render(NVGcontext* nvg)
        {
            auto growPosition = scrollBarThickness * 0.5f * growAnimation;
            auto growingBounds = thumbBounds.reduced(1).withTop(thumbBounds.getY() + growPosition);
            auto thumbCornerRadius = growingBounds.getHeight();
            auto fullBounds = growingBounds.withX(2).withWidth(getWidth() - 4);

            if (isVertical) {
                growingBounds = thumbBounds.reduced(1).withLeft(thumbBounds.getX() + growPosition);
                thumbCornerRadius = growingBounds.getWidth();
                fullBounds = growingBounds.withY(2).withHeight(getHeight() - 4);
            }

            scrollbarBgCol.a = (1.0f - growAnimation) * 150; // 0-150 opacity, not full opacity when active
            nvgDrawRoundedRect(nvg, fullBounds.getX(), fullBounds.getY(), fullBounds.getWidth(), fullBounds.getHeight(), scrollbarBgCol, scrollbarBgCol, thumbCornerRadius);

            auto scrollBarThumbCol = isMouseDragging ? activeScrollbarCol : scrollbarCol;
            nvgDrawRoundedRect(nvg, growingBounds.getX(), growingBounds.getY(), growingBounds.getWidth(), growingBounds.getHeight(), scrollBarThumbCol, scrollBarThumbCol, thumbCornerRadius);
        }

        NVGcolor scrollbarCol;
        NVGcolor activeScrollbarCol;
        NVGcolor scrollbarBgCol;

    private:
        bool isVertical = false;
        bool isMouseOver = false;
        bool isMouseDragging = false;

        float growAnimation = 1.0f;

        int scrollBarThickness = 0;

        Range<float> totalRange = { 0, 0 };
        Range<float> currentRange = { 0, 0 };
        Rectangle<float> thumbBounds = { 0, 0 };
        CanvasViewport* viewport;
        Point<int> viewPosition = { 0, 0 };
        FadeAnimator animator = FadeAnimator(this);
        FadeTimer fadeTimer;
    };

    struct ViewportPositioner : public Component::Positioner {
        explicit ViewportPositioner(Viewport& comp)
            : Component::Positioner(comp)
            , inset(comp.getScrollBarThickness())
        {
        }

        void applyNewBounds(Rectangle<int> const& newBounds) override
        {
            auto& component = getComponent();
            if (newBounds != component.getBounds()) {
                component.setBounds(newBounds.withTrimmedRight(-inset).withTrimmedBottom(-inset));
            }
        }

        int inset;
    };

public:
    CanvasViewport(PluginEditor* parent, Canvas* cnv)
        : NVGComponent(this)
        , editor(parent)
        , cnv(cnv)
    {
        setScrollBarsShown(false, false);

        setPositioner(new ViewportPositioner(*this));

#if JUCE_IOS
        setScrollOnDragMode(ScrollOnDragMode::never);
#endif

        setScrollBarThickness(8);

        addAndMakeVisible(vbar);
        addAndMakeVisible(hbar);

        setCachedComponentImage(new NVGSurface::InvalidationListener(editor->nvgSurface, this));

        lookAndFeelChanged();
    }

    ~CanvasViewport()
    {
    }

    void render(NVGcontext* nvg) override
    {
        {
            NVGScopedState scopedState(nvg);
            nvgTranslate(nvg, vbar.getX(), vbar.getY());
            vbar.render(nvg);
        }

        {
            NVGScopedState scopedState(nvg);
            nvgTranslate(nvg, hbar.getX(), hbar.getY());
            hbar.render(nvg);
        }
    }

    void timerCallback(int ID) override
    {
        switch (ID) {
            case Timers::ResizeTimer: {
                stopTimer(Timers::ResizeTimer);
                cnv->isZooming = false;

                // Cached geometry can look thicker/thinner at different zoom scales, so we update all cached connections when zooming is done
                if (scaleChanged) {
                    // Cached geometry can look thicker/thinner at different zoom scales, so we reset all cached connections when zooming is done
                    NVGCachedPath::resetAll();
                }

                scaleChanged = false;
                editor->nvgSurface.invalidateAll();
            } break;
            case Timers::AnimationTimer: {
                auto lerp = [](Point<int> start, Point<int> end, float t) {
                    return start.toFloat() + (end.toFloat() - start.toFloat()) * t;
                };
                auto movedPos = lerp(startPos, targetPos, lerpAnimation);
                setViewPosition(movedPos.x, movedPos.y);

                if (lerpAnimation >= 1.0f) {
                    stopTimer(Timers::AnimationTimer);
                    lerpAnimation = 0.0f;
                }

                lerpAnimation += animationSpeed;
            } break;
            case Timers::QuickCanvasTimer: {
                quickCanvasTimerCount = 0;
                stopTimer(Timers::QuickCanvasTimer);
            } break;
            case Timers::QuickCanvasAnimationTimer: {
                if (!cnv->quickCanvas)
                    return;

                float animationSpeed = 0.1f;

                if (quickCanvasShowingOrHiding) {
                    cnv->quickCanvas->quickCanvasAlpha = std::clamp(cnv->quickCanvas->quickCanvasAlpha + animationSpeed, 0.0f, 1.0f);

                    if (approximatelyEqual(1.0f, cnv->quickCanvas->quickCanvasAlpha))
                        stopTimer(Timers::QuickCanvasAnimationTimer);
                } else {
                    cnv->quickCanvas->quickCanvasAlpha = std::clamp(cnv->quickCanvas->quickCanvasAlpha - animationSpeed, 0.0f, 1.0f);

                    if (approximatelyEqual(0.0f, cnv->quickCanvas->quickCanvasAlpha)) {
                        stopTimer(Timers::QuickCanvasAnimationTimer);

                        //removeMouseListener(cnv->quickCanvas.get());
                        cnv->quickCanvas.reset();
                    }
                }
                cnv->repaint();
                if (cnv->quickCanvas) {
                    cnv->quickCanvas->repaint();
                }
            } break;
        }
    }

    void setViewPositionAnimated(Point<int> pos)
    {
        if (getViewPosition() != pos) {
            startPos = getViewPosition();
            targetPos = pos;
            lerpAnimation = 0.0f;
            auto distance = startPos.getDistanceFrom(pos) * getValue<float>(cnv->zoomScale);
            // speed up animation if we are traveling a shorter distance (hardcoded for now)
            animationSpeed = distance < 10.0f ? 0.1f : 0.02f;

            startTimer(Timers::AnimationTimer, 1000 / 90);
        }
    }

    void lookAndFeelChanged() override
    {
        auto scrollbarColour = hbar.findColour(ScrollBar::ColourIds::thumbColourId);
        auto scrollbarCol = convertColour(scrollbarColour);
        auto canvasBgColour = findColour(PlugDataColour::canvasBackgroundColourId);
        auto activeScrollbarCol = convertColour(scrollbarColour.interpolatedWith(canvasBgColour.contrasting(0.6f), 0.7f));
        auto scrollbarBgCol = convertColour(scrollbarColour.interpolatedWith(canvasBgColour, 0.7f));

        hbar.scrollbarCol = scrollbarCol;
        vbar.scrollbarCol = scrollbarCol;
        hbar.activeScrollbarCol = activeScrollbarCol;
        vbar.activeScrollbarCol = activeScrollbarCol;
        hbar.scrollbarBgCol = scrollbarBgCol;
        vbar.scrollbarBgCol = scrollbarBgCol;

        hbar.repaint();
        vbar.repaint();
    }

    void enableMousePanning(bool enablePanning, Canvas* canvas = nullptr)
    {
        panner.enablePanning(enablePanning, canvas);
    }

    bool hitTest(int x, int y) override
    {
        // needed so that mouseWheel event is registered in presentation mode
        return true;
    }

    void mouseMove(const MouseEvent& e) override
    {
        if (cnv->checkPanDragMode()) {
            for (auto obj : cnv->objects) {
                if (obj->getBounds().contains(e.getEventRelativeTo(cnv).getPosition())) {
                    if (auto patch = obj->gui->getPatch()) {
                        Image customCursorImage = ImageCache::getFromMemory(BinaryData::plugdata_logo_png, BinaryData::plugdata_logo_pngSize);
                        MouseCursor customCursor(customCursorImage, e.x, e.y);
                        setMouseCursor(customCursor);
                    }
                }
            }
        }
    }

    void mouseWheelMove(MouseEvent const& e, MouseWheelDetails const& wheel) override
    {
        // Check event time to filter out duplicate events
        // This is a workaround for a bug in JUCE that can cause mouse events to be duplicated when an object has a MouseListener on its parent
        if (e.eventTime == lastScrollTime)
            return;

        lastScrollTime = e.eventTime;

        if (cnv->checkPanDragMode()) {
            // Triger the quick view to show / close if mouse wheel is moved 2 times up / down within 1/10th of a second
            startTimer(Timers::QuickCanvasTimer, 1000 / 10);
            quickCanvasTimerCount++;
            if (quickCanvasTimerCount < 2)
                return;

            startTimer(Timers::QuickCanvasAnimationTimer, 1000 / 60);

            if (wheel.deltaY < 0.0f) {
                quickCanvasShowingOrHiding = false;
                return;
            }
            if (wheel.deltaY > 0.0f) {
                quickCanvasShowingOrHiding = true;
            }
            if (cnv->quickCanvas && quickCanvasShowingOrHiding) {
                // TODO: Enter quick canvas, replace the current canvas with the quick canvas completely
            }
            if (!cnv->quickCanvas && quickCanvasShowingOrHiding) {
                for (auto obj: cnv->objects) {
                    if (obj->getBounds().contains(e.getEventRelativeTo(cnv).getPosition())) {
                        if (auto patch = obj->gui->getPatch()) {
                            
                            quickCanvasTimerCount = 0;

                            cnv->quickCanvas = std::make_unique<Canvas>(editor, patch, nullptr, true);
                            cnv->addAndMakeVisible(cnv->quickCanvas.get());

                            cnv->quickCanvas->zoomScale.referTo(cnv->zoomScale);
                            cnv->quickCanvas->zoomScale.setValue(cnv->zoomScale);

                            cnv->quickCanvas->locked.setValue(cnv->locked);

                            //TODO: this causes duplicate events on quick canvas
                            //addMouseListener(cnv->quickCanvas.get(), true);

                            cnv->quickCanvas->quickCanvasOffset =
                                    cnv->canvasOrigin - obj->getPosition().translated(Object::margin, Object::margin) +
                                    patch->getGraphBounds().getPosition();

                            cnv->quickCanvas->grabKeyboardFocus();

                            cnv->resized();
                            return;
                        }
                    }
                }
            }
            return;
        }

        // Cancel the animation timer for the search panel
        stopTimer(Timers::AnimationTimer);

        if (e.mods.isCommandDown()) {
            mouseMagnify(e, 1.0f / (1.0f - wheel.deltaY));
        }

        Viewport::mouseWheelMove(e, wheel);
    }

    void mouseMagnify(MouseEvent const& e, float scrollFactor) override
    {
        // Check event time to filter out duplicate events
        // This is a workaround for a bug in JUCE that can cause mouse events to be duplicated when an object has a MouseListener on its parent
        if (e.eventTime == lastZoomTime || !cnv)
            return;

        // Apply and limit zoom
        magnify(std::clamp(getValue<float>(cnv->zoomScale) * scrollFactor, 0.25f, 3.0f));
        lastZoomTime = e.eventTime;
    }

    void magnify(float newScaleFactor)
    {
        if (approximatelyEqual(newScaleFactor, 0.0f)) {
            newScaleFactor = 1.0f;
        }

        if (newScaleFactor == lastScaleFactor) // float comparison ok here as it's set by the same value
            return;

        lastScaleFactor = newScaleFactor;

        scaleChanged = true;

        // Get floating point mouse position relative to screen
        auto mousePosition = Desktop::getInstance().getMainMouseSource().getScreenPosition();
        // Get mouse position relative to canvas
        auto oldPosition = cnv->getLocalPoint(nullptr, mousePosition);
        // Apply transform and make sure viewport bounds get updated
        cnv->setTransform(AffineTransform().scaled(newScaleFactor));
        // After zooming, get mouse position relative to canvas again
        auto newPosition = cnv->getLocalPoint(nullptr, mousePosition);
        // Calculate offset to keep our mouse position the same as before this zoom action
        auto offset = newPosition - oldPosition;
        cnv->setTopLeftPosition(cnv->getPosition() + offset.roundToInt());

        // This is needed to make sure the viewport the current canvas bounds to the lastVisibleArea variable
        // Without this, future calls to getViewPosition() will give wrong results
        resized();
        cnv->zoomScale = newScaleFactor;
    }

    void adjustScrollbarBounds()
    {
        if (getViewArea().isEmpty())
            return;

        auto thickness = getScrollBarThickness();
        auto localArea = getLocalBounds().reduced(2);

        vbar.setBounds(localArea.removeFromRight(thickness).withTrimmedBottom(thickness).translated(-1, 0));
        hbar.setBounds(localArea.removeFromBottom(thickness).translated(0, -1));

        float scale = 1.0f / std::sqrt(std::abs(cnv->getTransform().getDeterminant()));
        auto contentArea = getViewArea() * scale;

        Rectangle<int> objectArea = contentArea.withPosition(cnv->canvasOrigin);
        for (auto object : cnv->objects) {
            objectArea = objectArea.getUnion(object->getBounds());
        }

        auto totalArea = contentArea.getUnion(objectArea);

        hbar.setRangeLimitsAndCurrentRange(totalArea.getX(), totalArea.getRight(), contentArea.getX(), contentArea.getRight());
        vbar.setRangeLimitsAndCurrentRange(totalArea.getY(), totalArea.getBottom(), contentArea.getY(), contentArea.getBottom());
    }

    void componentMovedOrResized(Component& c, bool moved, bool resized) override
    {
        if (editor->isInPluginMode())
            return;

        Viewport::componentMovedOrResized(c, moved, resized);
        adjustScrollbarBounds();
    }

    void visibleAreaChanged(Rectangle<int> const& r) override
    {
        if (scaleChanged) {
            cnv->isZooming = true;
            startTimer(Timers::ResizeTimer, 150);
        }

        onScroll();
        adjustScrollbarBounds();
        editor->nvgSurface.invalidateAll();
    }

    void resized() override
    {
        vbar.setVisible(isVerticalScrollBarShown());
        hbar.setVisible(isHorizontalScrollBarShown());

        if (editor->isInPluginMode())
            return;

        adjustScrollbarBounds();

        if (!SettingsFile::getInstance()->getProperty<bool>("centre_resized_canvas")) {
            Viewport::resized();
            return;
        }

        float scale = std::sqrt(std::abs(cnv->getTransform().getDeterminant()));

        // centre canvas when resizing viewport
        auto getCentre = [this, scale](Rectangle<int> bounds) {
            if (scale > 1.0f) {
                auto point = cnv->getLocalPoint(this, bounds.withZeroOrigin().getCentre());
                return point * scale;
            }
            return getViewArea().withZeroOrigin().getCentre();
        };

        auto currentCentre = getCentre(previousBounds);
        previousBounds = getBounds();
        Viewport::resized();
        auto newCentre = getCentre(getBounds());

        auto offset = currentCentre - newCentre;
        setViewPosition(getViewPosition() + offset);
    }

    // Never respond to arrow keys, they have a different meaning
    bool keyPressed(KeyPress const& key) override
    {
        return false;
    }

    std::function<void()> onScroll = []() { };

private:
    enum Timers { ResizeTimer, AnimationTimer, QuickCanvasTimer, QuickCanvasAnimationTimer };
    int quickCanvasTimerCount = 0;
    bool quickCanvasShowingOrHiding = false;

    Point<int> startPos;
    Point<int> targetPos;
    float lerpAnimation;
    float animationSpeed;

    Time lastScrollTime;
    Time lastZoomTime;
    float lastScaleFactor = -1.0f;
    PluginEditor* editor;
    Canvas* cnv;
    Rectangle<int> previousBounds;
    MousePanner panner = MousePanner(this);
    ViewportScrollBar vbar = ViewportScrollBar(true, this);
    ViewportScrollBar hbar = ViewportScrollBar(false, this);
    bool scaleChanged = false;
};
