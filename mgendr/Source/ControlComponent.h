/*
  ==============================================================================

    ControlComponent.h
    Created: 6 Jun 2018 11:18:56pm
    Author:  adamson

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================
/*
*/
#ifdef USE_BURGER_MENU
// This class is adapted from the JUCE "MenusDemo"
class BurgerMenuHeader  : public Component
{
    public:
        BurgerMenuHeader(SidePanel& sp);
        ~BurgerMenuHeader();

    private:
        void paint (Graphics& g) override;

        void resized() override;

        void showOrHide()
            {sidePanel.showOrHide(!sidePanel.isPanelShowing());}

        SidePanel&  sidePanel;
        Label       titleLabel;
        ShapeButton burgerButton;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BurgerMenuHeader)
};  // end class BurgerMenuHeader
#endif // USE_BURGER_MENU
    
class ControlComponent    : public Component,
                            public Button::Listener,
                            public ComboBox::Listener,
                            public TextEditor::Listener,
                            public AsyncUpdater
{
    public:
        ControlComponent(class MgenderApp& theApp);
        ~ControlComponent();
        
        void paint (Graphics&) override;
        void resized() override;
        void buttonClicked(Button* buttonThatWasClicked) override;
    
        void comboBoxChanged(ComboBox* comboBoxThatHasChanged) override;
    
        void textEditorTextChanged(TextEditor &) override {}
        void textEditorReturnKeyPressed(TextEditor &editor) override;
        void textEditorEscapeKeyPressed(TextEditor &editor) override
            {textEditorReturnKeyPressed(editor);}
        void textEditorFocusLost(TextEditor &editor) override
            {textEditorReturnKeyPressed(editor);}
    
        void handleAsyncUpdate() override;
        
        void SetInterfaceText(const String& ifaceName)
            {iface_text.setText(ifaceName);}
    
        void SetReportContent(const String& theContent);  // to be called by mgen thread only
        void SetReportContentDirect(const String& theContent)
            {report_text.setText(theContent);}
        void UpdateProbeAddress();
        bool StartProbing();
        void OnUpdateProbeAddress();
    
        // Binary resources:
        static const char*  mgenLogo_png;
        static const int    mgenLogo_pngSize;

    private:
        class MgenderApp&           mgender;
        MenuBarComponent            menu_bar;
#ifdef USE_BURGER_MENU
        // Alternative "burger" menu for mobile devices
        // (copied from JUCE MenuDemo"
        BurgerMenuComponent         burger_menu;
        SidePanel                   menu_panel {"Menu", 300, false};
        BurgerMenuHeader            menu_header{menu_panel};
#endif // USE_BURGER_MENU    
        ImageButton                 logo_butn;
        GroupComponent              dest_group;
        TextEditor                  addr_text;
        Label                       addr_label;
        Label                       port_label;
        TextEditor                  port_text;
        Label                       iface_label;
        TextEditor                  iface_text;
        Label                       pattern_label;
        ComboBox                    pattern_box;
        Label                       rate_label;
        Label                       size_label;
        TextEditor                  rate_text;
        Label                       rate_units;
        TextEditor                  size_text;
        Label                       size_units;
        GroupComponent              report_group;
        TextEditor                  report_text;
        TextButton                  start_butn;
        TextButton                  stop_butn;
        ToggleButton                auto_toggle;

        FlexBox                     main_box;
        FlexBox                     logo_box;
        FlexBox                     flow_box;
        FlexBox                     report_box;
        FlexBox                     control_box;
        
        String                      report_content;
        bool                        report_content_pending;
        bool                        probe_addr_update_pending;
        CriticalSection             update_lock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlComponent)
                
};  // end class ControlComponent
