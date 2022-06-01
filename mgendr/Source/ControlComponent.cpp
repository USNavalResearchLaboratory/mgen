/*
  ==============================================================================

    ControlComponent.cpp
    Created: 6 Jun 2018 11:18:56pm
    Author:  adamson

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "ControlComponent.h"
#include "Main.h"

//==============================================================================

#ifdef USE_BURGER_MENU
BurgerMenuHeader::BurgerMenuHeader (SidePanel& sp)
    : sidePanel(sp), titleLabel("titleLabel", "mgendr"), 
      burgerButton("burgerButton", Colours::lightgrey, Colours::lightgrey, Colours::white)

{
    static const unsigned char burgerMenuPathData[]
        = { 110,109,0,0,128,64,0,0,32,65,108,0,0,224,65,0,0,32,65,98,254,212,232,65,0,0,32,65,0,0,240,65,252,
            169,17,65,0,0,240,65,0,0,0,65,98,0,0,240,65,8,172,220,64,254,212,232,65,0,0,192,64,0,0,224,65,0,0,
            192,64,108,0,0,128,64,0,0,192,64,98,16,88,57,64,0,0,192,64,0,0,0,64,8,172,220,64,0,0,0,64,0,0,0,65,
            98,0,0,0,64,252,169,17,65,16,88,57,64,0,0,32,65,0,0,128,64,0,0,32,65,99,109,0,0,224,65,0,0,96,65,108,
            0,0,128,64,0,0,96,65,98,16,88,57,64,0,0,96,65,0,0,0,64,4,86,110,65,0,0,0,64,0,0,128,65,98,0,0,0,64,
            254,212,136,65,16,88,57,64,0,0,144,65,0,0,128,64,0,0,144,65,108,0,0,224,65,0,0,144,65,98,254,212,232,
            65,0,0,144,65,0,0,240,65,254,212,136,65,0,0,240,65,0,0,128,65,98,0,0,240,65,4,86,110,65,254,212,232,
            65,0,0,96,65,0,0,224,65,0,0,96,65,99,109,0,0,224,65,0,0,176,65,108,0,0,128,64,0,0,176,65,98,16,88,57,
            64,0,0,176,65,0,0,0,64,2,43,183,65,0,0,0,64,0,0,192,65,98,0,0,0,64,254,212,200,65,16,88,57,64,0,0,208,
            65,0,0,128,64,0,0,208,65,108,0,0,224,65,0,0,208,65,98,254,212,232,65,0,0,208,65,0,0,240,65,254,212,
            200,65,0,0,240,65,0,0,192,65,98,0,0,240,65,2,43,183,65,254,212,232,65,0,0,176,65,0,0,224,65,0,0,176,
            65,99,101,0,0 };

    Path p;
    p.loadPathFromData (burgerMenuPathData, sizeof (burgerMenuPathData));
    burgerButton.setShape(p, true, true, false);
    burgerButton.onClick = [this] {showOrHide();};
    addAndMakeVisible(burgerButton);
}

BurgerMenuHeader::~BurgerMenuHeader()
{
    sidePanel.showOrHide(false);
}

void BurgerMenuHeader::paint(Graphics& g)
{
    auto titleBarBackgroundColour = getLookAndFeel().
                                    findColour (ResizableWindow::backgroundColourId).
                                    darker();
    g.setColour (titleBarBackgroundColour);
    g.fillRect (getLocalBounds());
}  // end BurgerMenuHeader::paint()

void BurgerMenuHeader::resized()
{
    auto r = getLocalBounds();

    burgerButton.setBounds (r.removeFromRight (40).withSizeKeepingCentre (20, 20));

    titleLabel.setFont (Font (getHeight() * 0.5f, Font::plain));
    titleLabel.setBounds (r);
}  // end BurgerMenuHeader::resized()
#endif // USE_BURGER_MENU

ControlComponent::ControlComponent(class MgenderApp& theApp)
  : mgender(theApp), menu_bar(&theApp), report_content_pending(false),
    probe_addr_update_pending(false)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    
#ifdef USE_BURGER_MENU
    burger_menu.setModel(&theApp);
    addAndMakeVisible(&menu_header);
    menu_panel.setContent(&burger_menu);
#else
    addAndMakeVisible(&menu_bar);
#endif // if/else USE_BURGER_MENU
    Colour groupLabelColor(0xff00ff00);
    
    // Our gratuitous MGEN logo button component
    logo_butn.setButtonText(TRANS("nuthin'"));
    logo_butn.addListener(this);
    logo_butn.setImages(false, true, true,
                        ImageCache::getFromMemory(mgenLogo_png, mgenLogo_pngSize), 1.000f,
                        Colour(0x00000000),
                        Image(), 1.000f, Colour(0x00000000),
                        Image(), 1.000f, Colour(0x00000000));

    addAndMakeVisible(&logo_butn);

    dest_group.setText(TRANS("Probing Configuration:"));
    dest_group.setColour(GroupComponent::outlineColourId, groupLabelColor);
    dest_group.setColour(GroupComponent::textColourId, groupLabelColor);
    addAndMakeVisible(&dest_group);
    
    //addr_label.reset(new Label("addr_label", TRANS("Address:")));
    addr_label.setText(TRANS("Addr:"), dontSendNotification);
    addr_label.setFont(Font(12.00f, Font::plain).withTypefaceStyle("Regular"));
    addr_label.setJustificationType(Justification::centredRight);
    addr_label.setEditable(false, false, false);
    addr_label.setColour(TextEditor::textColourId, Colours::white);
    addr_label.setColour(TextEditor::backgroundColourId, Colour(0x00000000));
    //addr_label.setBounds(16,(0 + proportionOfHeight(0.2000f)) + 24, 64, 32);
    dest_group.addAndMakeVisible(&addr_label);
    
    //addr_text.reset(new TextEditor("addr_text"));
    addr_text.setMultiLine(false);
    addr_text.setReturnKeyStartsNewLine(false);
    addr_text.setReadOnly(false);
    addr_text.setScrollbarsShown(true);
    addr_text.setCaretVisible(true);
    addr_text.setPopupMenuEnabled(true);
    addr_text.setText(TRANS(mgender.GetProbeAddress().GetHostString()));
    addr_text.addListener(this);
    dest_group.addAndMakeVisible(&addr_text);

    //port_label.reset(new Label("port_label", TRANS("Port:")));
    port_label.setText(TRANS("Port:"), dontSendNotification);
    port_label.setFont(Font(12.00f, Font::plain).withTypefaceStyle("Regular"));
    port_label.setJustificationType(Justification::centredRight);
    port_label.setEditable(false, false, false);
    port_label.setColour(TextEditor::textColourId, Colours::white);
    port_label.setColour(TextEditor::backgroundColourId, Colour(0x00000000));
    dest_group.addAndMakeVisible(&port_label);   
    
    //port_text.reset(new TextEditor("port_text"));
    port_text.setMultiLine(false);
    port_text.setReturnKeyStartsNewLine(false);
    port_text.setReadOnly(false);
    port_text.setScrollbarsShown(true);
    port_text.setCaretVisible(true);
    port_text.setPopupMenuEnabled(true);
    port_text.setText(String(mgender.GetProbeAddress().GetPort()));
    port_text.setKeyboardType(TextInputTarget::numericKeyboard);
    port_text.addListener(this);
    dest_group.addAndMakeVisible(&port_text);
    
    //iface_label.reset(new Label("iface_label", TRANS("Port:")));
    iface_label.setText(TRANS("Interface:"), dontSendNotification);
    iface_label.setFont(Font(12.00f, Font::plain).withTypefaceStyle("Regular"));
    iface_label.setJustificationType(Justification::centredRight);
    iface_label.setEditable(false, false, false);
    iface_label.setColour(TextEditor::textColourId, Colours::white);
    iface_label.setColour(TextEditor::backgroundColourId, Colour(0x00000000));
    dest_group.addAndMakeVisible(&iface_label);   
    
    //iface_text.reset(new TextEditor("iface_text"));
    iface_text.setMultiLine(false);
    iface_text.setReturnKeyStartsNewLine(false);
    iface_text.setReadOnly(false);
    iface_text.setScrollbarsShown(true);
    iface_text.setCaretVisible(true);
    iface_text.setPopupMenuEnabled(true);
    iface_text.setText(String(""));
    iface_text.setKeyboardType(TextInputTarget::numericKeyboard);
    iface_text.addListener(this);
    dest_group.addAndMakeVisible(&iface_text);
         
    //pattern_label.reset(new Label("pattern_label", TRANS("Message Pattern:")));
    pattern_label.setText(TRANS("Message Pattern:"), dontSendNotification);
    pattern_label.setFont(Font(12.00f, Font::plain).withTypefaceStyle("Regular"));
    pattern_label.setJustificationType(Justification::centredLeft);
    pattern_label.setEditable(false, false, false);
    pattern_label.setColour(TextEditor::textColourId, Colours::white);
    pattern_label.setColour(TextEditor::backgroundColourId, Colour(0x00000000));
    dest_group.addAndMakeVisible(&pattern_label);

    //pattern_box.reset(new ComboBox("pattern_box"));
    pattern_box.setTooltip(TRANS("MGEN pattern"));
    pattern_box.setEditableText(false);
    pattern_box.setJustificationType(Justification::centredLeft);
    pattern_box.setTextWhenNothingSelected(String());
    pattern_box.setTextWhenNoChoicesAvailable(TRANS("(no choices)"));
    pattern_box.addItem(TRANS("PERIODIC"), 1);
    pattern_box.addItem(TRANS("POISSON"), 2);
    pattern_box.setSelectedItemIndex(0, dontSendNotification);
    pattern_box.addListener(this);
    dest_group.addAndMakeVisible(&pattern_box);
    //pattern_label.attachToComponent(&pattern_box, false);

    // rate_label.reset(new Label("rate_abel", TRANS("Message Rate:")));
    rate_label.setText(TRANS("Message Rate:"), dontSendNotification);
    rate_label.setFont(Font(12.00f, Font::plain).withTypefaceStyle("Regular"));
    rate_label.setJustificationType(Justification::centredLeft);
    rate_label.setEditable(false, false, false);
    rate_label.setColour(TextEditor::textColourId, Colours::white);
    rate_label.setColour(TextEditor::backgroundColourId, Colour(0x00000000));
    dest_group.addAndMakeVisible(&rate_label);
    
    // rate_text.reset(new TextEditor("rate_text"));
    rate_text.setTooltip(TRANS("messages per second"));
    rate_text.setMultiLine(false);
    rate_text.setReturnKeyStartsNewLine(false);
    rate_text.setReadOnly(false);
    rate_text.setScrollbarsShown(true);
    rate_text.setCaretVisible(true);
    rate_text.setPopupMenuEnabled(true);
    rate_text.setText(TRANS("1.0"));
    rate_text.setKeyboardType(TextInputTarget::decimalKeyboard);
    rate_text.addListener(this);
    dest_group.addAndMakeVisible(&rate_text);
    //rate_label.attachToComponent(&rate_text, false);

    // size_label.reset(new Label("size_label", TRANS("Message Size:")));
    size_label.setText(TRANS("Message Size:"), dontSendNotification);
    size_label.setFont(Font(12.00f, Font::plain).withTypefaceStyle("Regular"));
    size_label.setJustificationType(Justification::centredLeft);
    size_label.setEditable(false, false, false);
    size_label.setColour(TextEditor::textColourId, Colours::white);
    size_label.setColour(TextEditor::backgroundColourId, Colour(0x00000000));
    dest_group.addAndMakeVisible(&size_label);
    
    //size_text.reset(new TextEditor("size_text"));
    size_text.setMultiLine(false);
    size_text.setReturnKeyStartsNewLine(false);
    size_text.setReadOnly(false);
    size_text.setScrollbarsShown(true);
    size_text.setCaretVisible(true);
    size_text.setPopupMenuEnabled(true);
    size_text.setText(TRANS("1024"));
    size_text.setKeyboardType(TextInputTarget::numericKeyboard);
    size_text.addListener(this);
    dest_group.addAndMakeVisible(&size_text);
    //size_label.attachToComponent(&size_text, false);
    
    report_group.setText(TRANS("Flow Reports:"));
    report_group.setColour(GroupComponent::outlineColourId, groupLabelColor);
    report_group.setColour(GroupComponent::textColourId, groupLabelColor);
    addAndMakeVisible(&report_group);
    
    //report_text.reset(new TextEditor("new text editor"));
    report_text.setMultiLine(true);
    report_text.setReturnKeyStartsNewLine(false);
    report_text.setReadOnly(true);
    report_text.setScrollbarsShown(true);
    report_text.setCaretVisible(true);
    report_text.setPopupMenuEnabled(true);
    report_text.setText(String());
    //report_text.setScrollToShowCursor(false);
    report_group.addAndMakeVisible(&report_text);
    
    Colour bkColor = getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    start_butn.setButtonText(TRANS("Start Probe"));
    start_butn.addListener(this);
    start_butn.setColour(TextButton::textColourOffId, bkColor);
    start_butn.setColour(TextButton::textColourOnId, Colours::black);
    start_butn.setColour(TextButton::buttonColourId, groupLabelColor);
    start_butn.setColour(TextButton::buttonOnColourId, Colour(0xff008000));
    addAndMakeVisible(&start_butn);
    
    stop_butn.setButtonText(TRANS("Stop Probe"));
    stop_butn.addListener(this);
    stop_butn.setColour(TextButton::textColourOffId, bkColor);
    stop_butn.setColour(TextButton::textColourOnId, Colours::black);
    stop_butn.setColour(TextButton::buttonColourId, groupLabelColor);
    stop_butn.setColour(TextButton::buttonOnColourId, Colour(0xff008000));
    addAndMakeVisible(&stop_butn);
    
    auto_toggle.setButtonText(TRANS("Auto Adjust Rate"));
    auto_toggle.addListener(this);
    auto_toggle.setColour(ToggleButton::tickColourId, groupLabelColor);
    auto_toggle.setColour(ToggleButton::tickDisabledColourId, groupLabelColor);
    auto_toggle.setColour(ToggleButton::textColourId, groupLabelColor);
    addAndMakeVisible(&auto_toggle);
    auto_toggle.setEnabled(false);  // not functional yet

#ifdef USE_BURGER_MENU       
    // We add this last so it ends up on top
    addAndMakeVisible(&menu_panel);
#endif // USE_BURGER_MENU
     
    setSize(420, 500);
}

ControlComponent::~ControlComponent()
{
}

void ControlComponent::paint(Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));   // clear the background
}

void ControlComponent::resized() {
    // This method is where you should set the bounds of any child
    // components that your component contains..
    FlexBox box;
    box.alignContent = FlexBox::AlignContent::flexStart;
    box.flexDirection = FlexBox::Direction::column;
    box.justifyContent = FlexBox::JustifyContent::flexStart;
    box.alignItems = FlexBox::AlignItems::flexStart;
    box.flexWrap = FlexBox::Wrap::noWrap;

    // Load our FlexBoxes with our GUI elements
    int usedHeight = 0;
    
#ifdef USE_BURGER_MENU
    box.items.add(FlexItem(getWidth(), 24, menu_header));
#else
    box.items.add(FlexItem(getWidth(), 24, menu_bar));
#endif  // if/else USE_BURGER_MENU
    
    usedHeight += 24;
    
#ifdef JUCE_ANDROID
    Desktop::DisplayOrientation orient = Desktop::getInstance().getCurrentOrientation();
    bool isPortrait = (Desktop::upright == orient) || (Desktop::upsideDown == orient);
    if (!isPortrait)
    {
        logo_butn.setVisible(false);
    }
    else
#endif // JUCE_ANDROID
    {
        logo_butn.setVisible(true);
        box.items.add(FlexItem(getWidth(), 64, logo_butn));
        usedHeight += 64;
    }
    
    int hr = getHeight() - usedHeight;  // height remaining minus MGEN logo
    
    // TBD - shoudl we even use a flex box for the address/pattern probe "destination"
    // group since it will be of fixed (non-flexed) height when all is said and done???
    FlexBox dboxa;  // flex box for dest addr/port configuration components
    dboxa.alignContent = FlexBox::AlignContent::flexStart;
    dboxa.flexDirection = FlexBox::Direction::row;
    dboxa.justifyContent = FlexBox::JustifyContent::flexStart;  // left-justified packing
    dboxa.alignItems = FlexBox::AlignItems::flexStart;
    dboxa.flexWrap = FlexBox::Wrap::noWrap;
    
    // Compute height (and width) for address components
    int lht = 1.25*addr_label.getFont().getHeight();
    int lwa = 1.25*addr_label.getFont().getStringWidth(addr_label.getText());
    int lwp = 1.25*port_label.getFont().getStringWidth(port_label.getText());
    int lwi = 1.25*iface_label.getFont().getStringWidth(iface_label.getText());
    dboxa.items.add(FlexItem(lwa, lht, addr_label).withFlex(0).withMinWidth(lwa).withMaxWidth(lwa));
    dboxa.items.add(FlexItem(1, lht, addr_text).withFlex(3));
    dboxa.items.add(FlexItem(lwp, lht, port_label).withFlex(0).withMinWidth(lwp).withMaxWidth(lwp));
    dboxa.items.add(FlexItem(1, lht, port_text).withFlex(1.25));
    dboxa.items.add(FlexItem(lwi, lht, iface_label).withFlex(0).withMinWidth(lwi).withMaxWidth(lwi));
    dboxa.items.add(FlexItem(1, lht, iface_text).withFlex(1.25));
    /*dboxa.items.add(FlexItem(lwa, 1.25*lht, addr_label));//.withMaxWidth(lwa));
    dboxa.items.add(FlexItem(edw, 1.25*lht, addr_text));//.withMaxWidth(2*lwa));
    dboxa.items.add(FlexItem(lwa, 1.25*lht, port_label).withMargin(FlexItem::Margin(0, 0, 0, 5)));//.withMaxWidth(lwp));
    dboxa.items.add(FlexItem(edw, 1.25*lht, port_text));//.withMaxWidth(lwa));
    dboxa.items.add(FlexItem(lwi, 1.25*lht, iface_label).withMargin(FlexItem::Margin(0, 0, 0, 5)));//.withMaxWidth(lwp));
    dboxa.items.add(FlexItem(edw, 1.25*lht, iface_text));//.withMaxWidth(lwa));*/
    
    // Flex boxes that stack flow parameter label/control component pairs of 'pattern', 'rate', and 'size'
    FlexBox dboxpt;  // destination group pattern tyoe FlexBox
    dboxpt.alignContent = FlexBox::AlignContent::flexStart;
    dboxpt.flexDirection = FlexBox::Direction::column;
    dboxpt.justifyContent = FlexBox::JustifyContent::flexStart;  // top-justified packing
    dboxpt.alignItems = FlexBox::AlignItems::stretch;
    dboxpt.flexWrap = FlexBox::Wrap::noWrap;
    int lwpp = pattern_label.getFont().getStringWidth(pattern_label.getText());
    dboxpt.items.add(FlexItem(lwpp, lht, pattern_label));
    dboxpt.items.add(FlexItem(1, lht, pattern_box).withFlex(1.0));
    
    
    FlexBox dboxpr;  // destination group pattern rate FlexBox
    dboxpr.alignContent = FlexBox::AlignContent::flexStart;
    dboxpr.flexDirection = FlexBox::Direction::column;
    dboxpr.justifyContent = FlexBox::JustifyContent::flexStart;  // top-justified packing
    dboxpr.alignItems = FlexBox::AlignItems::stretch;
    dboxpr.flexWrap = FlexBox::Wrap::noWrap;
    int lwpr = rate_label.getFont().getStringWidth(pattern_label.getText());
    dboxpr.items.add(FlexItem(lwpr, lht, rate_label));
    dboxpr.items.add(FlexItem(lwpr, lht, rate_text));
   
    FlexBox dboxps;   // destination group pattern size FlexBox
    dboxps.alignContent = FlexBox::AlignContent::flexStart;
    dboxps.flexDirection = FlexBox::Direction::column;
    dboxps.justifyContent = FlexBox::JustifyContent::flexStart;  // top-justified packing
    dboxps.alignItems = FlexBox::AlignItems::stretch;
    dboxps.flexWrap = FlexBox::Wrap::noWrap;
    int lwps = size_label.getFont().getStringWidth(pattern_label.getText());
    dboxps.items.add(FlexItem(lwps, lht, size_label).withFlex(1.0));
    dboxps.items.add(FlexItem(lwps, lht, size_text).withFlex(1.0));
    
    
    // Pack pattern config components into a horizontal (row) flex box
    FlexBox dboxp;  // flex box for destination group patterm configuration components
    dboxp.alignContent = FlexBox::AlignContent::flexStart;
    dboxp.flexDirection = FlexBox::Direction::row;
    dboxp.justifyContent = FlexBox::JustifyContent::flexStart;  // left-justified packing
    dboxp.alignItems = FlexBox::AlignItems::flexStart;
    dboxp.flexWrap = FlexBox::Wrap::noWrap;
    /*dboxp.items.addArray({FlexItem(1, 2.5*lht, dboxpt).withFlex(1).withMargin(5),
                          FlexItem(1, 2.5*lht, dboxpr).withFlex(1).withMargin(5),
                          FlexItem(1, 2.5*lht, dboxps).withFlex(1).withMargin(5)});*/
    /*
    dboxp.items.add(FlexItem(lwpp, 2.5*lht, dboxpt).withFlex(1.0));//.withMargin(5));
    dboxp.items.add(FlexItem(lwpr, 2.5*lht, dboxpr).withFlex(1.0));//.withMargin(5));
    dboxp.items.add(FlexItem(lwps, 2.5*lht, dboxps).withFlex(1.0));//.withMargin(5));
    */
    dboxp.items.add(FlexItem(1, 1, dboxpt).withFlex(1).withMargin(5));
    dboxp.items.add(FlexItem(1, 1, dboxpr).withFlex(1).withMargin(5));
    dboxp.items.add(FlexItem(1, 1, dboxps).withFlex(1).withMargin(5));
    
    FlexBox dboxd;  // Stack destination address and pattern config boxes
    dboxd.alignContent = FlexBox::AlignContent::flexStart;
    dboxd.flexDirection = FlexBox::Direction::column;
    dboxd.justifyContent = FlexBox::JustifyContent::flexStart;  // top-justified packing
    dboxd.alignItems = FlexBox::AlignItems::flexStart;
    dboxd.flexWrap = FlexBox::Wrap::noWrap;
    // note Margin(left, right, top, bottom)
    // note Margin(top, ?, ?
    dboxd.items.addArray({FlexItem(getWidth() - 10, 1, dboxa).withMargin(FlexItem::Margin(15, 0, 0, 0)),
                          FlexItem(getWidth() - 10, 1, dboxp).withMargin(FlexItem::Margin(15, 0, 0, 0))});
    // 'dboxd' will get layed out later after dest_group layout size is determined
    
    box.items.add(FlexItem(getWidth(), lht*6, dest_group));
    box.items.add(FlexItem(getWidth(), 0.8*(hr - lht*6),report_group));
    
    FlexBox cbox;  // 'control' box with start/stop buttons in a horizontal row
    cbox.alignContent = FlexBox::AlignContent::flexStart;
    cbox.flexDirection = FlexBox::Direction::row;
    cbox.justifyContent = FlexBox::JustifyContent::spaceAround;
    cbox.alignItems = FlexBox::AlignItems::center;
    cbox.flexWrap = FlexBox::Wrap::wrap;
    int butnHt = 2*Font().getHeight();
    cbox.items.add(FlexItem(0.3*getWidth(), butnHt, start_butn).withMargin(5));
    cbox.items.add(FlexItem(0.3*getWidth(), butnHt, stop_butn).withMargin(5));
    
    box.items.add(FlexItem(getWidth(), butnHt, cbox).withMargin(0));
    
    int aw = 2*Font().getStringWidth(auto_toggle.getButtonText());
    box.items.add(FlexItem(aw, butnHt, auto_toggle).withMaxWidth(aw).withMargin(5));
    
    box.performLayout(getLocalBounds());
    
    dboxd.performLayout(dest_group.getLocalBounds().reduced(5));
    report_text.setBounds(report_group.getLocalBounds().reduced(15));
}

void ControlComponent::buttonClicked(Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]
    
    if (buttonThatWasClicked == &logo_butn)
    {
        //[UserButtonCode_logo_butn] -- add your button handler code here..
        //[/UserButtonCode_logo_butn]
    }
    else if ((buttonThatWasClicked == &start_butn) && !mgender.IsProbing())
    {
        // Colors the button as "set" during DNS lookup
        Colour c = start_butn.findColour(TextButton::buttonOnColourId);
        start_butn.setColour(TextButton::buttonColourId, c);
        if (StartProbing())
        {
            start_butn.setButtonText(TRANS("Probing ..."));
            // Disable flow pattern config controls
            addr_text.setEnabled(false);
            port_text.setEnabled(false);
            iface_text.setEnabled(false);
        }
        else
        {
            // Restore to "unset" state
            Colour c = stop_butn.findColour(TextButton::buttonColourId);
            start_butn.setColour(TextButton::buttonColourId, c);
        }
    }
    else if (buttonThatWasClicked == &stop_butn)
    {
        mgender.StopProbing();
        Colour c = stop_butn.findColour(TextButton::buttonColourId);
        start_butn.setColour(TextButton::buttonColourId, c);
        start_butn.setButtonText(TRANS("Start Probe"));
        // Reenable flow pattern config controls
        addr_text.setEnabled(true);
        port_text.setEnabled(true);
        iface_text.setEnabled(true);
    }
    else if (buttonThatWasClicked == &auto_toggle)
    {
        //[UserButtonCode_auto_toggle] -- add your button handler code here..
        //[/UserButtonCode_auto_toggle]
    }
    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

void ControlComponent::comboBoxChanged(ComboBox* comboBoxThatHasChanged)
{
    //[UsercomboBoxChanged_Pre]
    //[/UsercomboBoxChanged_Pre]
    
    if(comboBoxThatHasChanged == &pattern_box)
    {
        // If probing is active, update probing parameters
        // (TBD - add code to check for actual param change?)
        if (mgender.IsProbing()) StartProbing();
    }
    
    //[UsercomboBoxChanged_Post]
    //[/UsercomboBoxChanged_Post]
}

void ControlComponent::textEditorReturnKeyPressed(TextEditor& editor)
{
    if ((&editor == &port_text) || (&editor == &addr_text) || (&editor == &iface_text))
    {
        ProtoAddress probeAddr;
        if (!probeAddr.ResolveFromString(addr_text.getText().toRawUTF8()))
        {
            String warn = String("Invalid probe address \"") + addr_text.getText() + String("\"");
#ifdef ANDROID
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
        }
        const char* ifaceName = (0 != iface_text.getText().length()) ? iface_text.getText().toRawUTF8() : NULL;
        char ifaceNameText[256];
        ifaceNameText[255] = '\0';
        if (NULL != ifaceName)
        {
            strncpy(ifaceNameText, ifaceName, 255);
            ifaceName = ifaceNameText;
        }
        int portNum = port_text.getText().getIntValue();
        if ((portNum >= 0) && (portNum <= 65535))
        {   
            // Only post "listening" changed alert when germane information is changed
            probeAddr.SetPort(portNum);
            // Note a zero value disables probe listening and sending
            bool change = !mgender.GetProbeAddress().IsEqual(probeAddr);
            if (NULL == ifaceName)
                change |= (NULL != mgender.GetInterface());
            else if (NULL == mgender.GetInterface())
                change = true;
            else
                change |= (0 != strcmp(ifaceName, mgender.GetInterface()));
            if (change && mgender.SetProbeAddress(probeAddr, ifaceName))
            {
                bool multicastInvolved = probeAddr.IsMulticast() || mgender.GetProbeAddress().IsMulticast();
                bool alert = multicastInvolved ? change : (portNum != mgender.GetProbeAddress().GetPort());
                if (alert)
                {
                    String info = (0 == portNum) ?
                    String("MGEN probing disabled.") :
                    String("Now listening for MGEN probes on port ") + String(portNum);
#ifdef ANDROID
                    AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon, String("mgender"), info);
#else
                    AlertWindow::showMessageBox(AlertWindow::InfoIcon, String("mgender"), info);
#endif  // if/else ANDROID
                }
            }
            else if (change)
            {
                String warn = String("Unable to listen on port ") + String(portNum);
#ifdef ANDROID
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
                AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
                port_text.setText(String("0"));
            }
        }
        else
        {
            String warn = String("Invalid probe port \"") + port_text.getText() + String("\"");
#ifdef ANDROID
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
            port_text.setText(String("0"));
        }
    }
    else if ((&editor == &rate_text) || (&editor == &size_text))
    {
        // If probing is active, update probing parameters
        // (TBD - add code to check for actual param change?)
        if (mgender.IsProbing()) StartProbing();
    }
}  // ControlComponent::textEditorReturnKeyPressed()


void ControlComponent::SetReportContent(const String& theContent)
{
    update_lock.enter();
    report_content = theContent;
    report_content_pending = true;
    ControlComponent::triggerAsyncUpdate();
    update_lock.exit();
}  // end ControlComponent::SetReportContent()

void ControlComponent::UpdateProbeAddress()
{
    update_lock.enter();
    probe_addr_update_pending = true;
    ControlComponent::triggerAsyncUpdate();
    update_lock.exit();
}  // end ControlComponent::UpdateProbeAddress(

void ControlComponent::OnUpdateProbeAddress()
{
    const ProtoAddress& probeAddr = mgender.GetProbeAddress();
    addr_text.setText(String(probeAddr.GetHostString()));
    port_text.setText(String(probeAddr.GetPort()));
    if (NULL != mgender.GetInterface())
        iface_text.setText(String(mgender.GetInterface()));
    else
        iface_text.setText(String(""));
    if (mgender.IsProbing()) StartProbing();  // to update probe info
}  // end ControlComponent::OnUpdateProbeAddress()

void ControlComponent::handleAsyncUpdate()
{
    update_lock.enter();
    if (report_content_pending)
    {
        int count = report_text.getTotalNumChars();
        int cpos = report_text.getCaretPosition();
        double relpos = (0 != count) ? (double)cpos / (double)count : 1.0;
        int npos = relpos * report_content.length();
        report_text.setText(report_content);
        report_text.setCaretPosition(npos);
        report_content_pending = false;
    }
    if (probe_addr_update_pending) OnUpdateProbeAddress();
    update_lock.exit();
}  // end ControlComponent::handleAsyncUpdate()

bool ControlComponent::StartProbing()
{
    // Get and validate probing parameters from GUI
    // TBD - show a modal dialog during long DNS lookup?
    ProtoAddress probeAddr;
    if (!probeAddr.ResolveFromString(addr_text.getText().toRawUTF8()))
    {
        String warn = String("Invalid probe destination address \"") + addr_text.getText() + String("\"");
#ifdef ANDROID
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
        return false;
    } 
    probeAddr.SetPort(mgender.GetProbeAddress().GetPort());
    const char* pattern = pattern_box.getText().toRawUTF8();
    double rate = rate_text.getText().getFloatValue();
    if (rate <= 0.0)
    {
        String warn = String("Invalid probe message rate \"") + rate_text.getText() + String("\"");
#ifdef ANDROID
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
        return false;
    }
    unsigned int sizeMin = (ProtoAddress::IPv4 == probeAddr.GetType()) ? 28 : 40;
    int size = size_text.getText().getIntValue();
    if ((size < sizeMin)  || (size > 8192))
    {
        String warn = String("Invalid probe message size \"") + size_text.getText() + String("\"");
#ifdef ANDROID
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
        return false;
    }
    const char* ifaceName = NULL;
    if (probeAddr.IsMulticast() && (0 != iface_text.getText().length()))
        ifaceName = iface_text.getText().toRawUTF8();
    if (!mgender.StartProbing(probeAddr, pattern, rate, size, ifaceName))
    {
        String warn = String("Probing startup failed!\n");
#ifdef ANDROID
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, String("mgender"), warn);
#else
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, String("mgender"), warn);
#endif  // if/else ANDROID
        return false;
    }
    return true;
    
}  // end ControlComponent::StartProbing()
        
// JUCER_RESOURCE: mgenLogo_png, 2520, "../Resources/mgenLogo.png"
static const unsigned char resource_ControlComponent_mgenLogo_png[] = { 137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,64,0,0,0,64,8,6,0,0,0,170,105,113,222,0,0,0,9,112,72,89,115,0,0,11,19,0,0,11,19,
    1,0,154,156,24,0,0,0,2,73,68,65,84,120,218,253,27,117,142,0,0,9,124,73,68,65,84,229,251,45,112,235,216,21,199,1,152,154,85,80,176,102,43,246,196,106,88,177,53,91,177,39,248,204,214,44,98,49,140,89,196,
    214,44,130,49,75,88,204,106,24,179,152,116,98,150,128,118,230,121,250,185,157,206,180,233,43,56,123,206,185,247,92,29,201,114,226,68,242,91,191,73,103,254,245,91,251,90,247,252,127,247,220,111,167,3,0,
    239,90,141,31,240,85,197,225,182,240,156,111,18,0,133,250,238,1,188,91,0,221,46,128,143,234,116,223,33,0,49,31,160,186,239,21,192,129,204,183,3,224,0,253,114,171,245,163,99,7,112,40,8,158,135,230,173,
    142,23,128,109,165,182,3,36,243,33,42,182,162,255,110,250,204,131,0,32,29,162,159,122,202,252,187,3,224,251,0,137,213,8,149,126,11,0,66,59,88,53,13,148,204,251,202,188,136,222,107,26,231,225,1,236,145,
    174,47,25,241,123,133,249,177,210,241,3,240,246,4,128,6,73,117,159,245,240,253,8,53,12,0,38,189,66,71,15,128,228,41,0,207,165,44,153,236,237,0,16,4,133,249,169,82,246,12,180,163,3,144,188,208,111,165,
    149,201,108,213,252,15,223,25,0,83,173,103,128,181,160,189,10,117,113,132,247,48,197,171,210,101,252,190,15,189,126,207,41,136,108,250,86,130,15,7,33,84,5,97,31,224,251,15,104,62,228,239,84,63,167,103,
    105,201,179,116,125,213,152,119,197,249,54,0,56,186,179,6,70,222,192,99,149,0,196,62,136,122,49,6,149,244,74,173,183,152,207,33,68,131,117,98,0,244,58,70,229,33,132,91,101,2,8,146,66,14,0,214,163,235,
    45,1,176,49,82,188,237,0,136,11,121,137,81,9,192,16,131,64,245,70,61,35,1,64,105,140,233,205,0,210,144,213,31,23,138,198,125,136,82,4,112,242,59,236,235,248,154,247,221,103,82,62,76,209,56,105,100,228,
    0,216,186,164,238,18,0,27,35,197,219,14,128,137,239,228,225,160,68,42,1,152,4,32,162,160,130,81,15,116,95,38,19,253,60,42,41,202,7,172,120,138,154,68,108,30,174,251,12,69,151,11,167,40,132,67,98,0,52,
    94,244,44,232,105,232,234,45,1,176,49,118,83,175,61,0,222,60,44,169,4,96,209,135,94,30,22,0,18,109,62,100,19,209,34,134,193,34,49,90,38,16,207,98,86,66,202,6,104,30,33,44,12,12,250,156,202,139,28,4,4,
    201,131,104,100,1,204,35,22,213,95,2,128,245,50,132,212,111,1,192,192,102,192,170,15,30,105,29,177,116,25,14,196,2,8,56,3,202,0,216,252,227,144,149,44,11,13,231,9,140,174,19,24,78,99,54,79,74,16,64,252,
    56,50,229,23,6,132,203,132,212,78,149,129,169,167,183,30,148,84,2,64,106,29,128,53,191,11,64,144,133,69,95,37,0,153,233,207,98,158,140,137,209,116,157,194,104,54,132,116,62,132,211,203,79,0,75,132,176,
    26,192,104,50,128,33,150,163,178,63,220,126,228,110,193,221,99,108,7,74,7,0,181,138,225,87,3,224,47,11,8,85,0,52,114,147,121,26,213,169,79,139,249,209,99,106,0,160,217,236,113,2,217,34,101,229,215,248,
    254,61,66,88,39,48,206,18,134,67,16,40,75,184,107,16,0,122,30,9,205,223,220,220,124,125,0,58,253,125,5,64,250,97,111,214,55,0,210,192,76,111,12,192,12,106,100,126,40,0,208,36,105,186,201,32,95,77,32,95,
    142,225,234,230,28,96,131,159,109,70,112,126,241,35,140,31,199,92,118,184,80,0,88,6,0,103,64,90,3,192,110,166,124,13,160,219,45,139,206,48,236,65,78,171,0,130,188,30,64,108,251,52,1,160,150,37,147,164,
    217,99,6,119,119,23,240,184,206,97,189,204,1,158,140,40,27,8,0,41,93,141,12,128,89,84,0,32,77,66,7,64,67,144,109,116,9,64,100,119,170,125,165,55,1,208,233,255,12,0,26,168,104,224,147,121,93,0,80,139,146,
    41,206,0,4,240,184,153,193,195,195,13,60,124,190,133,135,251,27,124,212,2,1,204,25,70,142,112,168,139,140,151,8,96,110,103,136,107,149,5,19,187,62,168,0,224,152,70,21,0,195,174,81,172,178,224,53,93,128,
    87,85,52,165,40,243,174,50,76,55,54,143,253,154,0,152,169,42,52,139,155,73,159,91,46,193,65,77,204,83,218,115,170,255,156,193,253,231,43,248,203,191,254,136,85,252,27,190,252,231,31,248,250,55,212,18,
    214,171,107,88,255,156,115,89,6,176,50,131,102,209,21,76,118,237,4,144,213,0,232,214,31,216,52,207,128,100,27,0,175,240,118,0,224,12,32,0,48,229,214,6,248,51,5,245,133,254,207,252,111,141,25,96,0,72,6,
    140,104,186,164,53,195,116,96,178,96,110,151,204,251,2,120,102,123,254,250,49,96,101,22,28,84,145,6,160,187,128,44,101,185,181,44,0,234,255,100,158,76,193,63,79,1,254,123,105,82,30,254,68,207,251,159,
    49,255,127,7,128,186,7,141,17,147,85,90,6,176,196,103,46,67,86,9,192,114,192,146,172,244,15,50,11,32,121,89,113,209,43,255,219,2,160,185,89,50,32,146,46,64,3,23,246,223,97,22,195,201,221,9,155,167,190,
    77,233,143,77,105,1,252,213,26,39,253,189,212,5,8,192,249,253,41,15,132,14,192,186,2,128,50,111,97,140,59,81,108,135,2,32,75,96,1,32,173,79,0,194,153,89,174,242,6,103,140,239,93,126,95,0,184,61,129,179,
    187,83,248,9,13,193,19,117,129,115,11,97,201,173,110,180,228,247,104,16,188,123,184,224,41,146,50,128,1,228,9,47,153,119,1,224,189,129,93,18,31,4,0,15,130,215,33,75,0,208,188,207,115,255,181,2,64,107,
    126,1,96,51,128,214,249,52,159,83,127,62,199,76,192,194,240,0,247,240,249,225,10,103,128,63,224,76,112,203,173,110,128,76,225,254,230,140,205,211,2,73,198,128,196,238,27,182,0,96,221,92,63,1,152,89,243,
    179,176,93,0,50,11,252,230,226,59,126,56,3,160,74,80,12,128,250,122,80,211,5,232,253,165,153,6,9,0,181,228,100,62,98,0,151,119,103,112,117,123,6,247,15,151,188,14,184,191,61,103,243,0,167,112,129,11,33,
    50,79,101,105,153,76,251,5,2,64,59,71,13,128,215,1,185,173,31,183,222,188,23,177,113,29,4,128,222,6,83,5,12,32,55,16,232,36,71,15,130,188,14,160,1,107,101,118,119,100,128,140,156,222,124,98,0,188,252,
    197,21,224,37,26,191,184,57,49,75,97,238,26,67,56,251,233,147,51,79,27,37,216,96,11,111,250,198,252,218,152,135,121,224,0,72,253,28,203,212,152,111,21,128,219,11,216,179,0,154,86,244,254,159,247,226,153,
    217,155,203,66,136,103,129,165,201,128,136,22,66,56,128,17,4,54,244,20,179,65,217,7,156,225,70,232,236,194,128,161,207,120,47,128,155,164,31,47,63,154,126,79,0,84,203,243,52,56,179,155,161,204,152,166,
    250,117,60,174,193,90,7,144,154,155,26,175,82,33,207,189,147,192,2,176,211,224,210,64,224,49,65,65,32,147,188,11,180,26,225,0,71,162,247,249,51,90,55,228,102,139,204,35,191,180,254,178,104,125,13,64,234,
    167,227,243,18,128,212,111,0,64,110,125,187,246,44,16,77,147,28,0,251,240,58,0,102,47,16,184,128,205,160,88,64,128,13,234,105,192,35,187,24,165,181,2,189,71,250,116,246,123,103,126,48,142,202,169,95,2,
    208,43,213,47,0,36,182,215,3,16,211,36,186,235,235,119,120,227,224,206,4,71,30,139,0,208,43,3,24,218,74,50,117,30,40,7,34,110,180,46,32,176,33,238,211,102,118,32,185,35,177,167,62,139,14,68,168,156,124,
    167,232,247,214,252,92,29,137,89,185,93,160,141,71,98,229,120,95,4,160,141,215,168,122,40,74,99,128,60,220,29,72,250,229,67,81,6,48,51,146,65,177,48,212,103,8,100,82,75,0,72,57,183,162,84,3,159,59,103,
    84,0,244,129,40,253,91,98,107,247,80,84,3,0,115,234,170,33,72,229,117,167,194,180,53,214,39,189,108,104,83,24,165,41,147,167,205,141,81,245,84,24,214,1,232,212,231,231,78,234,1,112,188,214,252,65,1,8,
    4,125,68,206,231,243,4,33,209,0,204,1,169,134,224,0,164,178,102,176,247,3,246,253,234,157,64,9,192,229,111,11,0,137,61,18,143,235,143,196,219,5,96,47,69,74,0,236,229,195,213,213,149,153,26,229,98,68,0,
    100,22,64,28,148,46,54,234,46,71,62,124,252,96,140,174,205,0,42,210,223,11,70,234,185,153,169,231,185,75,145,118,1,168,219,161,106,69,114,47,168,111,105,56,40,26,153,51,123,165,21,107,0,133,57,109,214,
    173,240,146,96,75,2,149,1,228,158,153,117,246,184,21,106,231,102,40,196,7,133,5,132,45,0,246,106,220,143,252,146,220,143,29,48,88,130,192,87,102,177,130,17,151,37,35,188,126,79,190,163,229,0,232,186,234,
    0,84,226,109,14,192,202,85,36,151,143,246,199,17,91,0,168,156,5,64,65,179,121,201,134,29,230,75,0,236,101,104,61,0,15,118,1,144,216,170,241,182,11,160,91,220,24,203,113,19,221,16,107,113,215,24,21,0,224,
    218,220,32,107,115,124,219,155,245,74,191,7,112,198,251,219,98,179,2,64,215,117,80,0,184,40,210,218,2,160,42,221,2,16,155,96,75,0,162,202,181,182,111,77,89,213,25,103,243,242,220,10,128,186,43,112,137,
    205,197,219,4,192,155,229,89,243,153,87,50,200,210,63,158,144,110,162,165,63,247,60,216,250,190,60,55,237,54,250,117,90,115,147,187,36,191,27,170,3,80,61,164,244,204,242,218,149,205,118,148,209,207,152,
    116,141,142,26,0,205,193,217,11,198,234,178,101,87,57,41,35,230,143,30,192,176,91,14,246,185,95,143,73,22,188,244,163,200,110,23,142,31,128,252,114,244,53,0,52,132,215,2,104,240,135,20,205,205,214,169,
    99,239,226,52,128,125,90,73,178,230,165,178,26,66,218,49,122,227,47,214,155,155,173,5,208,41,0,188,54,69,187,123,150,239,40,243,71,5,64,14,83,6,248,154,188,33,176,189,1,116,142,24,0,157,36,13,222,24,212,
    190,0,164,174,228,141,160,143,22,128,60,227,53,101,27,64,104,110,184,26,140,111,207,18,155,252,25,205,107,191,219,121,59,240,230,166,235,0,28,234,111,136,190,9,0,95,219,124,195,186,155,87,124,12,230,27,
    196,208,188,82,93,121,211,103,180,161,189,227,128,14,0,116,126,1,193,253,108,60,158,125,29,79,0,0,0,0,73,69,78,68,174,66,96,130,0,0};

const char* ControlComponent::mgenLogo_png = (const char*)resource_ControlComponent_mgenLogo_png;
const int ControlComponent::mgenLogo_pngSize = 2520;


