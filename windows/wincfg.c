/*
 * wincfg.c - the Windows-specific parts of the PuTTY configuration
 * box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

static void about_handler(union control *ctrl, void *dlg,
			  void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->generic.context.p;

    if (event == EVENT_ACTION) {
	modal_about_box(*hwndp);
    }
}

static void help_handler(union control *ctrl, void *dlg,
			 void *data, int event)
{
    HWND *hwndp = (HWND *)ctrl->generic.context.p;

    if (event == EVENT_ACTION) {
	show_help(*hwndp);
    }
}

/*
 * HACK: PuttyTray / Session Icon
 */ 
static void window_icon_handler(union control *ctrl, void *dlg, void *data, int event)
{
    Conf *conf = (Conf *) data;

    if (event == EVENT_ACTION) {
	char buf[512], iname[512], *ipointer;
	int iindex;

	memset(&iname, 0, sizeof(iname));
	memset(&buf, 0, sizeof(buf));
	iindex = 0;
	ipointer = iname;
	if (dlg_pick_icon(dlg, &ipointer, sizeof(iname), &iindex) /*&& iname[0]*/) {
	    if (iname[0]) {
		sprintf(buf, "%s,%d", iname, iindex);
	    } else {
		sprintf(buf, "%s", iname);
	    }
	    dlg_icon_set((union control *) ctrl->button.context.p, dlg, buf);
	    conf_set_str(conf, CONF_win_icon, buf);
	};
    };
};

static void variable_pitch_handler(union control *ctrl, void *dlg,
                                   void *data, int event)
{
    if (event == EVENT_REFRESH) {
	dlg_checkbox_set(ctrl, dlg, !dlg_get_fixed_pitch_flag(dlg));
    } else if (event == EVENT_VALCHANGE) {
	dlg_set_fixed_pitch_flag(dlg, !dlg_checkbox_get(ctrl, dlg));
    }
}

void win_setup_config_box(struct controlbox *b, HWND *hwndp, int has_help,
			  int midsession, int protocol)
{
    struct controlset *s;
    union control *c;
    char *str;

    if (!midsession) {
	/*
	 * Add the About and Help buttons to the standard panel.
	 */
	s = ctrl_getset(b, "", "", "");
	c = ctrl_pushbutton(s, "����", 'a', HELPCTX(no_help),
			    about_handler, P(hwndp));
	c->generic.column = 0;
	if (has_help) {
	    c = ctrl_pushbutton(s, "����", 'h', HELPCTX(no_help),
				help_handler, P(hwndp));
	    c->generic.column = 1;
	}
    }

    /*
     * Full-screen mode is a Windows peculiarity; hence
     * scrollbar_in_fullscreen is as well.
     */
    s = ctrl_getset(b, "â", "scrollback",
		    "â ���� ���� �÷����� ����");
    ctrl_checkbox(s, "��ü ȭ�� ��忡�� ��ũ�� ���̱�", 'i',
		  HELPCTX(window_scrollback),
		  conf_checkbox_handler,
		  I(CONF_scrollbar_in_fullscreen));
    /*
     * Really this wants to go just after `Display scrollbar'. See
     * if we can find that control, and do some shuffling.
     */
    {
        int i;
        for (i = 0; i < s->ncontrols; i++) {
            c = s->ctrls[i];
            if (c->generic.type == CTRL_CHECKBOX &&
                c->generic.context.i == CONF_scrollbar) {
                /*
                 * Control i is the scrollbar checkbox.
                 * Control s->ncontrols-1 is the scrollbar-in-FS one.
                 */
                if (i < s->ncontrols-2) {
                    c = s->ctrls[s->ncontrols-1];
                    memmove(s->ctrls+i+2, s->ctrls+i+1,
                            (s->ncontrols-i-2)*sizeof(union control *));
                    s->ctrls[i+1] = c;
                }
                break;
            }
        }
    }

    /*
     * Windows has the AltGr key, which has various Windows-
     * specific options.
     */
    s = ctrl_getset(b, "�͹̳�/Ű����", "features",
		    "Ű���� �ΰ� ���:");
    ctrl_checkbox(s, "ȸ�� Alt(AltGr)Ű�� ���� Ű�� ���", 't',
		  HELPCTX(keyboard_compose),
		  conf_checkbox_handler, I(CONF_compose_key));
    ctrl_checkbox(s, "Ctrl-Alt�� ȸ�� Alt(AltGr)Ű�� ����", 'd',
		  HELPCTX(keyboard_ctrlalt),
		  conf_checkbox_handler, I(CONF_ctrlaltkeys));
#ifdef SUPPORT_CYGTERM
    ctrl_checkbox(s, "(Escape Ű ���) AltŰ�� meta bit�� ����", NO_SHORTCUT,
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_alt_metabit));
#endif

    /*
     * Windows allows an arbitrary .WAV to be played as a bell, and
     * also the use of the PC speaker. For this we must search the
     * existing controlset for the radio-button set controlling the
     * `beep' option, and add extra buttons to it.
     * 
     * Note that although this _looks_ like a hideous hack, it's
     * actually all above board. The well-defined interface to the
     * per-platform dialog box code is the _data structures_ `union
     * control', `struct controlset' and so on; so code like this
     * that reaches into those data structures and changes bits of
     * them is perfectly legitimate and crosses no boundaries. All
     * the ctrl_* routines that create most of the controls are
     * convenient shortcuts provided on the cross-platform side of
     * the interface, and template creation code is under no actual
     * obligation to use them.
     */
    s = ctrl_getset(b, "�͹̳�/��", "style", "�� ��Ÿ�� ����");
    {
	int i;
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == CONF_beep) {
		assert(c->generic.handler == conf_radiobutton_handler);
		c->radio.nbuttons += 2;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("�ٸ� ���� ���� ���");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("PC ����Ŀ ������ ���");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-1] = I(BELL_WAVEFILE);
		c->radio.buttondata[c->radio.nbuttons-2] = I(BELL_PCSPEAKER);
		if (c->radio.shortcuts) {
		    c->radio.shortcuts =
			sresize(c->radio.shortcuts, c->radio.nbuttons, char);
		    c->radio.shortcuts[c->radio.nbuttons-1] = NO_SHORTCUT;
		    c->radio.shortcuts[c->radio.nbuttons-2] = NO_SHORTCUT;
		}
		break;
	    }
	}
    }
    ctrl_filesel(s, "�� �Ҹ��� ����� ���� ����:", NO_SHORTCUT,
		 FILTER_WAVE_FILES, FALSE, "�� �Ҹ� ���� ����",
		 HELPCTX(bell_style),
		 conf_filesel_handler, I(CONF_bell_wavefile));

    /*
     * While we've got this box open, taskbar flashing on a bell is
     * also Windows-specific.
     */
    ctrl_radiobuttons(s, "���� �︮�� �۾�ǥ���ٿ� ǥ��:", 'i', 3,
		      HELPCTX(bell_taskbar),
		      conf_radiobutton_handler,
		      I(CONF_beep_ind),
		      "��� �� ��", I(B_IND_DISABLED),
		      "������", I(B_IND_FLASH),
		      "���", I(B_IND_STEADY), NULL);

    /*
     * The sunken-edge border is a Windows GUI feature.
     */
    s = ctrl_getset(b, "â/���", "border",
		    "â �׵θ� ����");
    ctrl_checkbox(s, "Ƣ��� âƲ(�� �� ���Ϳ�)", 's',
		  HELPCTX(appearance_border),
		  conf_checkbox_handler, I(CONF_sunken_edge));

    s = ctrl_getset(b, "â/���", "font",
		    "��Ʈ ����");
    /*
     * HACK: iPuTTY
     */
    ctrl_checkbox(s, "�����ڵ�� ������ �۲� ���", 'f',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_use_font_unicode));
    ctrl_fontsel(s, "�����ڵ�� �۲�", 's',
		 HELPCTX(no_help),
		 conf_fontsel_handler, I(CONF_font_unicode));
    ctrl_editbox(s, "�����ڵ� ��Ʈ ����(px)", 'a', 20,
		 HELPCTX(no_help),
		 conf_editbox_handler, I(CONF_font_unicode_adj), I(-1));
    ctrl_checkbox(s, "������ �۲� ���� ���", NO_SHORTCUT,
                  HELPCTX(appearance_font), variable_pitch_handler, I(0));
    /*
     * Configurable font quality settings for Windows.
     */
    ctrl_radiobuttons(s, "�۲� ǰ��:", 'q', 2,
		      HELPCTX(appearance_font),
		      conf_radiobutton_handler,
		      I(CONF_font_quality),
		      "Antialiased", I(FQ_ANTIALIASED),
		      "Non-Antialiased", I(FQ_NONANTIALIASED),
		      "ClearType", I(FQ_CLEARTYPE),
		      "�⺻", I(FQ_DEFAULT), NULL);

    /*
     * Cyrillic Lock is a horrid misfeature even on Windows, and
     * the least we can do is ensure it never makes it to any other
     * platform (at least unless someone fixes it!).
     */
    s = ctrl_getset(b, "â/��ȯ", "tweaks", NULL);
    ctrl_checkbox(s, "CapsLock�� ��ƾ/Ű�� ��ȯŰ�� ���", 's',
		  HELPCTX(translation_cyrillic),
		  conf_checkbox_handler,
		  I(CONF_xlat_capslockcyr));

    /*
     * On Windows we can use but not enumerate translation tables
     * from the operating system. Briefly document this.
     */
    s = ctrl_getset(b, "â/��ȯ", "trans",
		    "������ �������� ���ڼ�");
    ctrl_text(s, "(Windows���� �����ϴ� �ڵ������� ������ ��Ͽ� ���� �� �ֽ��ϴ�. "
	      "���� ��� CP866�� ���� ���� �Է��Ͽ� ����� �� �ֽ��ϴ�.)",
	      HELPCTX(translation_codepage));

    /*
     * Windows has the weird OEM font mode, which gives us some
     * additional options when working with line-drawing
     * characters.
     */
    str = dupprintf("%s �� �׸��� ó�� ���", appname);
    s = ctrl_getset(b, "â/��ȯ", "linedraw", str);
    sfree(str);
    {
	int i;
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == CONF_vtmode) {
		assert(c->generic.handler == conf_radiobutton_handler);
		c->radio.nbuttons += 3;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-3] =
		    dupstr("X������ ���ڵ� ���");
		c->radio.buttons[c->radio.nbuttons-2] =
		    dupstr("ANSI�� OEM ��� ��� ���");
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("OEM ��� �۲ø� ���");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-3] = I(VT_XWINDOWS);
		c->radio.buttondata[c->radio.nbuttons-2] = I(VT_OEMANSI);
		c->radio.buttondata[c->radio.nbuttons-1] = I(VT_OEMONLY);
		if (!c->radio.shortcuts) {
		    int j;
		    c->radio.shortcuts = snewn(c->radio.nbuttons, char);
		    for (j = 0; j < c->radio.nbuttons; j++)
			c->radio.shortcuts[j] = NO_SHORTCUT;
		} else {
		    c->radio.shortcuts = sresize(c->radio.shortcuts,
						 c->radio.nbuttons, char);
		}
		c->radio.shortcuts[c->radio.nbuttons-3] = 'x';
		c->radio.shortcuts[c->radio.nbuttons-2] = 'b';
		c->radio.shortcuts[c->radio.nbuttons-1] = 'e';
		break;
	    }
	}
    }

    /*
     * RTF paste is Windows-specific.
     */
    s = ctrl_getset(b, "â/����", "format",
		    "�ٿ��ֱ� ����");
    ctrl_checkbox(s, "Ŭ������� �ؽ�Ʈ�� RTF�� ���ÿ� ����", 'f',
		  HELPCTX(selection_rtf),
		  conf_checkbox_handler, I(CONF_rtf_paste));

    /*
     * Windows often has no middle button, so we supply a selection
     * mode in which the more critical Paste action is available on
     * the right button instead.
     */
    s = ctrl_getset(b, "â/����", "mouse",
		    "���콺 ����");
    ctrl_radiobuttons(s, "���콺 ��ư ����:", 'm', 1,
		      HELPCTX(selection_buttons),
		      conf_radiobutton_handler,
		      I(CONF_mouse_is_xterm),
		      "������ (�� Ȯ��, ���� �޴� ���)", I(2),
		      "Ÿ�� (�� Ȯ��, ���� �ٿ��ֱ�)", I(0),
		      "xterm (���� Ȯ��, �� �ٿ��ֱ�)", I(1), NULL);
    /*
     * This really ought to go at the _top_ of its box, not the
     * bottom, so we'll just do some shuffling now we've set it
     * up...
     */
    c = s->ctrls[s->ncontrols-1];      /* this should be the new control */
    memmove(s->ctrls+1, s->ctrls, (s->ncontrols-1)*sizeof(union control *));
    s->ctrls[0] = c;

    /*
     * Logical palettes don't even make sense anywhere except Windows.
     */
    s = ctrl_getset(b, "â/����", "general",
		    "���� ���� �ɼ�");
    ctrl_checkbox(s, "�� �ķ�Ʈ ��� �õ�", 'l',
		  HELPCTX(colours_logpal),
		  conf_checkbox_handler, I(CONF_try_palette));
    ctrl_checkbox(s, "�ý��� ���� ���", 's',
                  HELPCTX(colours_system),
                  conf_checkbox_handler, I(CONF_system_colour));


    /*
     * Resize-by-changing-font is a Windows insanity.
     */
    s = ctrl_getset(b, "â", "size", "Set the size of the window");
    ctrl_radiobuttons(s, "â ũ�� ���� ��:", 'z', 1,
		      HELPCTX(window_resize),
		      conf_radiobutton_handler,
		      I(CONF_resize_action),
		      "���� ���� ũ�� ����", I(RESIZE_TERM),
		      "�۲� ũ�� ����", I(RESIZE_FONT),
		      "�ִ�ȭ �Ǿ��� ���� �۲� ũ�� ����", I(RESIZE_EITHER),
		      "ũ�� ������ �� ��", I(RESIZE_DISABLED), NULL);

    /*
     * Most of the Window/Behaviour stuff is there to mimic Windows
     * conventions which PuTTY can optionally disregard. Hence,
     * most of these options are Windows-specific.
     */
    s = ctrl_getset(b, "â/Ư��", "main", NULL);
    ctrl_checkbox(s, "Alt-F4�� ������ â�� ����", '4',
		  HELPCTX(behaviour_altf4),
		  conf_checkbox_handler, I(CONF_alt_f4));
    ctrl_checkbox(s, "Alt-Space�� ������ �ý��� �޴� ����", 'm',  /* HACK: PuttyTray: Changed shortcut key */
		  HELPCTX(behaviour_altspace),
		  conf_checkbox_handler, I(CONF_alt_space));
    ctrl_checkbox(s, "Alt�� ������ �ý��� �޴� ����", 'l',
		  HELPCTX(behaviour_altonly),
		  conf_checkbox_handler, I(CONF_alt_only));
    ctrl_checkbox(s, "â �׻� �� ����", 'e',
		  HELPCTX(behaviour_alwaysontop),
		  conf_checkbox_handler, I(CONF_alwaysontop));
    ctrl_checkbox(s, "Alt-Enter�� ������  ��ü ȭ������ ��ȯ", 'f',
		  HELPCTX(behaviour_altenter),
		  conf_checkbox_handler,
		  I(CONF_fullscreenonaltenter));

    /*
     * HACK: PuttyTray
     */
    ctrl_radiobuttons(s, "Ʈ���� ������ ���̱�:", NO_SHORTCUT, 4,
		      HELPCTX(no_help),
		      conf_radiobutton_handler,
		      I(CONF_tray),
		      "����", 'n', I(TRAY_NORMAL),
		      "�׻�", 'y', I(TRAY_ALWAYS),
		      "����", 'r', I(TRAY_NEVER),
		      "���� ��", 's', I(TRAY_START), NULL);
    ctrl_checkbox(s, "Ʈ���� ������ Ŭ�� �� â ��ȯ", 'm',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_tray_restore));

    /*
     * HACK: PuttyTray / Session Icon
     */
    s = ctrl_getset(b, "â/Ư��", "icon", "������ ����");
    ctrl_columns(s, 3, 40, 20, 40);
    c = ctrl_text(s, "â / Ʈ���� ������:", HELPCTX(appearance_title));
    c->generic.column = 0;
    c = ctrl_icon(s, HELPCTX(appearance_title),
		  I(CONF_win_icon));
    c->generic.column = 1;
    c = ctrl_pushbutton(s, "������ ����...", 'h', HELPCTX(appearance_title),
			window_icon_handler, P(c));
    c->generic.column = 2;
    ctrl_columns(s, 1, 100);

    /*
     * HACK: PuttyTray / Transparency
     */
    s = ctrl_getset(b, "â", "main", "â ���� �ɼ�");
    ctrl_editbox(s, "���� (50-255)", 't', 30, HELPCTX(no_help), conf_editbox_handler, I(CONF_transparency), I(-1));

    /*
     * HACK: PuttyTray / Reconnect
     */
    s = ctrl_getset(b, "����", "reconnect", "������ �ɼ�");
    ctrl_checkbox(s, "���� ��忡�� ��� �ÿ� ������", 'w', HELPCTX(no_help), conf_checkbox_handler, I(CONF_wakeup_reconnect));
    ctrl_checkbox(s, "���� ���н� ������", 'w', HELPCTX(no_help), conf_checkbox_handler, I(CONF_failure_reconnect));

    /*
     * HACK: PuttyTray / Nutty
     * Hyperlink stuff: The Window/Hyperlinks panel.
     */
    ctrl_settitle(b, "â/�����۸�ũ", "�����۸�ũ ���� ����");
    s = ctrl_getset(b, "â/�����۸�ũ", "general", "�����۸�ũ ����");

    ctrl_radiobuttons(s, "�����۸�ũ:", NO_SHORTCUT, 1,
		      HELPCTX(no_help),
		      conf_radiobutton_handler,
		      I(CONF_url_underline),
		      "�׻�", NO_SHORTCUT, I(URLHACK_UNDERLINE_ALWAYS),
		      "���콺 ������", NO_SHORTCUT, I(URLHACK_UNDERLINE_HOVER),
		      "������� ����", NO_SHORTCUT, I(URLHACK_UNDERLINE_NEVER),
		      NULL);

    ctrl_checkbox(s, "ctrl+click ���� �����۸�ũ ����", 'l',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_url_ctrl_click));

    s = ctrl_getset(b, "â/�����۸�ũ", "browser", "������ ����");

    ctrl_checkbox(s, "�⺻ ������ ���", 'b',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_url_defbrowser));

    ctrl_filesel(s, "�Ǵ� �ٸ� ���ø����̼����� �����۸�ũ ����:", 's',
		 "�������α׷� (*.exe)\0*.exe\0��� ���� (*.*)\0*.*\0\0", TRUE,
		 "�����۸�ũ�� �������� �������� ����", HELPCTX(no_help),
		 conf_filesel_handler, I(CONF_url_browser));

    s = ctrl_getset(b, "â/�����۸�ũ", "regexp", "���� ǥ����");

    ctrl_checkbox(s, "�⺻ ���� ǥ���� ���", 'r',
		  HELPCTX(no_help),
		  conf_checkbox_handler, I(CONF_url_defregex));

    ctrl_editbox(s, "�Ǵ� ����� ����:", NO_SHORTCUT, 100,
		 HELPCTX(no_help),
		 conf_editbox_handler, I(CONF_url_regex),
		 I(1));

    ctrl_text(s, "�ϳ��� ���鹮�ڰ� ��ũ �տ� ������ ��� ���ŵ˴ϴ�.",
	      HELPCTX(no_help));

    /*
     * Windows supports a local-command proxy. This also means we
     * must adjust the text on the `Telnet command' control.
     */
    if (!midsession) {
	int i;
        s = ctrl_getset(b, "����/���Ͻ�", "basics", NULL);
	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_RADIO &&
		c->generic.context.i == CONF_proxy_type) {
		assert(c->generic.handler == conf_radiobutton_handler);
		c->radio.nbuttons++;
		c->radio.buttons =
		    sresize(c->radio.buttons, c->radio.nbuttons, char *);
		c->radio.buttons[c->radio.nbuttons-1] =
		    dupstr("Local");
		c->radio.buttondata =
		    sresize(c->radio.buttondata, c->radio.nbuttons, intorptr);
		c->radio.buttondata[c->radio.nbuttons-1] = I(PROXY_CMD);
		break;
	    }
	}

	for (i = 0; i < s->ncontrols; i++) {
	    c = s->ctrls[i];
	    if (c->generic.type == CTRL_EDITBOX &&
		c->generic.context.i == CONF_proxy_telnet_command) {
		assert(c->generic.handler == conf_editbox_handler);
		sfree(c->generic.label);
		c->generic.label = dupstr("Telnet ��� �Ǵ� ����"
					  " ���Ͻ� ���");
		break;
	    }
	}
    }

    /*
     * Serial back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_SERIAL))
        ser_setup_config_box(b, midsession, 0x1F, 0x0F);

    /*
     * $XAUTHORITY is not reliable on Windows, so we provide a
     * means to override it.
     */
    if (!midsession && backend_from_proto(PROT_SSH)) {
	s = ctrl_getset(b, "����/SSH/X11", "x11", "X11 ������");
	ctrl_filesel(s, "���� ����� ���� X ���� ����", 't',
		     NULL, FALSE, "X ���� ���� ����",
		     HELPCTX(ssh_tunnels_xauthority),
		     conf_filesel_handler, I(CONF_xauthfile));
    }
#ifdef SUPPORT_CYGTERM
    /*
     * cygterm back end is available on Windows.
     */
    if (!midsession || (protocol == PROT_CYGTERM))
	cygterm_setup_config_box(b, midsession);
#endif
}

// vim: ts=8 sts=4 sw=4 noet cino=\:2\=2(0u0
