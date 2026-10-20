#include "PianoRoll.h"
using namespace juce;
using namespace ui;

PianoRoll::PianoRoll (GraphProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (MouseCursor::CrosshairCursor);
    startTimerHz (25);
}

double PianoRoll::gridSeconds() const
{
    const double beat = 60.0 / bpm;
    switch ((int) proc.apvts.getRawParameterValue ("grid")->load()) { case 0: return beat; case 1: return beat / 2; case 2: return beat / 4; default: return 0.0; }
}
double PianoRoll::snapTime (double t) const
{
    const double g = gridSeconds();
    return jmax (0.0, g > 0.0 ? std::round (t / g) * g : t);
}
bool PianoRoll::inScale (int pc) const
{
    static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 }, minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load(), sc = (int) proc.apvts.getRawParameterValue ("scale")->load();
    if (sc == 2) return true;
    const int rel = ((pc - key) % 12 + 12) % 12;
    for (int i = 0; i < 7; ++i) if ((sc == 0 ? major[i] : minor[i]) == rel) return true;
    return false;
}
int PianoRoll::snapPitch (int m) const
{
    if (proc.apvts.getRawParameterValue ("snap")->load() < 0.5f) return m;
    for (int d = 0; d <= 6; ++d)
    {
        if (inScale (((m + d) % 12 + 12) % 12)) return m + d;
        if (inScale (((m - d) % 12 + 12) % 12)) return m - d;
    }
    return m;
}
Rectangle<float> PianoRoll::noteRect (const GraphNote& n) const
{
    return { xOfTime (n.start), yOfMidi (n.pitch) + 1.0f, (float) (n.length * pps), rowH() - 2.0f };
}
int PianoRoll::hitNote (Point<float> p) const
{
    for (int i = (int) proc.notes.size() - 1; i >= 0; --i)
        if (noteRect (proc.notes[(size_t) i]).expanded (0.0f, 1.0f).contains (p)) return i;
    return -1;
}

void PianoRoll::timerCallback()
{
    const int len = proc.capturedSamples();
    if (len != lastLen)
    {
        const ScopedLock sl (proc.captureLock);
        const float* d = proc.captureData();
        if (len < lastLen) { lastLen = 0; normPeak = 0.05f; }
        for (int i = lastLen; i < len; ++i) normPeak = jmax (normPeak, std::abs (d[i]));
        lastLen = len;
        repaint();
    }
}

void PianoRoll::drawWaveform (Graphics& g)
{
    const ScopedLock sl (proc.captureLock);
    const float* d = proc.captureData();
    const int n = proc.capturedSamples();
    const double sr = proc.captureRate();
    if (n <= 0) return;

    const float mid = rulerH + (getHeight() - rulerH) * 0.5f, half = (getHeight() - rulerH) * 0.30f / normPeak;
    std::vector<std::pair<float, float>> pk;
    std::vector<float> xs;
    for (int x = keyW; x < getWidth(); ++x)
    {
        const int i0 = jmax (0, (int) (timeAtX ((float) x) * sr)), i1 = jmin (n, (int) (timeAtX ((float) x + 1.0f) * sr));
        if (i0 >= n) break;
        if (i1 <= i0) continue;
        const int stride = jmax (1, (i1 - i0) / 24);
        float mn = 0.0f, mx = 0.0f;
        for (int i = i0; i < i1; i += stride) { mn = jmin (mn, d[i]); mx = jmax (mx, d[i]); }
        pk.push_back ({ mn, mx }); xs.push_back ((float) x);
    }
    if (pk.empty()) return;
    Path p;
    p.startNewSubPath (xs[0], mid - pk[0].second * half);
    for (size_t i = 1; i < pk.size(); ++i) p.lineTo (xs[i], mid - jmax (pk[i].second, 0.4f / half) * half);
    for (size_t i = pk.size(); i-- > 0;) p.lineTo (xs[i], mid - jmin (pk[i].first, -0.4f / half) * half);
    p.closeSubPath();
    g.setColour (Colour (0xff8d97a8).withAlpha (0.38f)); g.fillPath (p);
}

void PianoRoll::paint (Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0e1117)); g.fillRoundedRectangle (all, 10.0f);
    Path clip; clip.addRoundedRectangle (all, 10.0f); g.saveState(); g.reduceClipRegion (clip);

    const float rh = rowH(), w = (float) getWidth();
    // rows
    for (int m = topMidi; m > topMidi - rows; --m)
    {
        const int pc = ((m % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const auto r = Rectangle<float> ((float) keyW, yOfMidi (m), w - keyW, rh);
        g.setColour (Colour (black ? 0xff0e1117 : 0xff141820)); g.fillRect (r);
        if (inScale (pc) && ! black) { g.setColour (accent.withAlpha (0.07f)); g.fillRect (r); }
        g.setColour (Colour (0xff222833)); g.drawHorizontalLine ((int) r.getY(), r.getX(), w);
    }
    // time grid
    const double beat = 60.0 / bpm, gs = gridSeconds();
    const double tEnd = timeAtX (w);
    if (gs > 0.0)
        for (double t = std::floor (viewStart / gs) * gs; t < tEnd; t += gs)
            if (xOfTime (t) >= keyW) { g.setColour (Colour (0xff1b212b)); g.drawVerticalLine ((int) xOfTime (t), (float) rulerH, (float) getHeight()); }
    for (double t = std::floor (viewStart / beat) * beat; t < tEnd; t += beat)
    {
        const float x = xOfTime (t);
        if (x < keyW) continue;
        const bool bar = std::fmod (std::round (t / beat), 4.0) == 0.0;
        g.setColour (Colour (bar ? 0xff2a3140 : 0xff202631)); g.drawVerticalLine ((int) x, (float) rulerH, (float) getHeight());
    }

    g.saveState(); g.reduceClipRegion (keyW, rulerH, getWidth() - keyW, getHeight() - rulerH);
    if (proc.apvts.getRawParameterValue ("wave")->load() > 0.5f) drawWaveform (g);

    // notes
    for (int i = 0; i < (int) proc.notes.size(); ++i)
    {
        const auto r = noteRect (proc.notes[(size_t) i]);
        const bool sel = i == selected;
        g.setColour (accent.withAlpha (sel ? 0.42f : 0.26f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (accent); g.drawRoundedRectangle (r, 4.0f, sel ? 2.0f : 1.2f);
        if (sel) { g.setColour (Colours::white.withAlpha (0.35f)); g.drawRoundedRectangle (r.expanded (3.0f), 6.0f, 1.0f); }
    }
    g.restoreState();

    // key column + ruler
    g.setColour (panelBot); g.fillRect (0, rulerH, keyW, getHeight() - rulerH);
    for (int m = topMidi; m > topMidi - rows; --m)
    {
        const int pc = ((m % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour (black ? mute : text); g.setFont (F (10.5f, ! black));
        g.drawText (GraphProcessor::keyNames()[pc] + String (m / 12 - 1), Rectangle<float> (12.0f, yOfMidi (m), 38.0f, rh), Justification::centredLeft);
    }
    g.setColour (Colour (0xff11141a)); g.fillRect (0, 0, getWidth(), rulerH);
    g.setColour (mute); g.setFont (F (10.0f));
    for (double t = std::floor (viewStart / (4 * beat)) * 4 * beat; t < tEnd; t += 4 * beat)
        if (xOfTime (t) >= keyW) g.drawText (String ((int) std::round (t / (4 * beat)) + 1), Rectangle<float> (xOfTime (t) + 5, 0, 40, (float) rulerH), Justification::centredLeft);
    g.restoreState();
}

void PianoRoll::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    if (e.x < keyW || e.y < rulerH) return;
    const int hit = hitNote (e.position);
    const double t = timeAtX (e.position.x);
    drag = None;

    if (tool == Erase) { if (hit >= 0) { proc.notes.erase (proc.notes.begin() + hit); select (-1); } return; }
    if (tool == Split)
    {
        if (hit >= 0)
        {
            auto& n = proc.notes[(size_t) hit];
            const double at = snapTime (t);
            if (at > n.start + 0.03 && at < n.start + n.length - 0.03)
            {
                GraphNote b = n; b.start = at; b.length = n.start + n.length - at; n.length = at - n.start;
                proc.notes.insert (proc.notes.begin() + hit + 1, b); select (hit + 1);
            }
        }
        return;
    }
    if (hit >= 0)
    {
        select (hit);
        auto& n = proc.notes[(size_t) hit];
        const auto r = noteRect (n);
        if (e.position.x > r.getRight() - 7.0f) drag = Resize;
        else { drag = Move; dragOffT = t - n.start; dragOffPitch = midiAtY (e.position.y) - n.pitch; }
        return;
    }
    if (tool == Draw)
    {
        GraphNote n;
        n.start = snapTime (t); n.pitch = snapPitch (midiAtY (e.position.y));
        n.length = gridSeconds() > 0.0 ? gridSeconds() : 0.25;
        proc.notes.push_back (n); select ((int) proc.notes.size() - 1); drag = Resize;
    }
    else select (-1);
}

void PianoRoll::mouseDrag (const MouseEvent& e)
{
    auto* n = selectedNote();
    if (n == nullptr || drag == None) return;
    const double t = timeAtX (e.position.x);
    if (drag == Resize) n->length = jmax (gridSeconds() > 0.0 ? gridSeconds() : 0.05, snapTime (t) - n->start);
    else
    {
        n->start = snapTime (t - dragOffT);
        n->pitch = snapPitch (midiAtY (e.position.y) - dragOffPitch);
    }
    repaint();
}

void PianoRoll::mouseUp (const MouseEvent&) { drag = None; }

void PianoRoll::mouseMove (const MouseEvent& e)
{
    if (tool == Select)
    {
        const int h = hitNote (e.position);
        if (h >= 0) { setMouseCursor (e.position.x > noteRect (proc.notes[(size_t) h]).getRight() - 7.0f ? MouseCursor::LeftRightResizeCursor : MouseCursor::DraggingHandCursor); return; }
        setMouseCursor (MouseCursor::NormalCursor);
    }
    else setMouseCursor (tool == Erase ? MouseCursor::PointingHandCursor : MouseCursor::CrosshairCursor);
}

void PianoRoll::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        const double anchor = timeAtX ((float) e.x);
        pps = jlimit (20.0, 600.0, pps * (1.0 + w.deltaY * 0.6));
        viewStart = jmax (0.0, anchor - (e.x - keyW) / pps);
    }
    else viewStart = jmax (0.0, viewStart - (w.deltaX + w.deltaY) * 1.5 * (90.0 / pps));
    repaint();
}

void PianoRoll::deleteSelected()
{
    if (selectedNote() == nullptr) return;
    proc.notes.erase (proc.notes.begin() + selected);
    selected = -1; if (onSelectionChanged) onSelectionChanged(); repaint();
}

bool PianoRoll::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey) { deleteSelected(); return true; }
    return false;
}
