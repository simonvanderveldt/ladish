/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef LOADPATCHWINDOW_H
#define LOADPATCHWINDOW_H

#include <libglademm/xml.h>
#include <gtkmm.h>
#include "raul/SharedPtr.hpp"
#include "interface/GraphObject.hpp"
#include "client/PluginModel.hpp"
#include "client/PatchModel.hpp"
using Ingen::Client::PatchModel;
using namespace Ingen::Shared;

namespace Ingen {
namespace GUI {
	

/** 'Load Patch' window.
 *
 * Loaded by glade as a derived object.  Used for both "Import" and "Load"
 * (e.g. Replace, clear-then-import) operations (the radio button state
 * should be changed with the provided methods before presenting).
 *
 * This is not for loading subpatches.  See @a LoadSubpatchWindow for that.
 *
 * \ingroup GUI
 */
class LoadPatchWindow : public Gtk::FileChooserDialog
{
public:
	LoadPatchWindow(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& xml);

	void set_patch(SharedPtr<PatchModel> patch);

	void set_replace() { _replace = true; }
	void set_merge()   { _replace = false; }

	void present(SharedPtr<PatchModel> patch, GraphObject::Variables data);

protected:
	void on_show();

private:
	void poly_from_file_selected();
	void poly_from_user_selected();
	void ok_clicked();
	void cancel_clicked();

	GraphObject::Variables _initial_data;

	SharedPtr<PatchModel> _patch;
	bool                  _replace;

	Gtk::RadioButton* _poly_from_current_radio;
	Gtk::RadioButton* _poly_from_file_radio;
	Gtk::RadioButton* _poly_from_user_radio;
	Gtk::SpinButton*  _poly_spinbutton;
	Gtk::Button*      _ok_button;
	Gtk::Button*      _cancel_button;
};
 

} // namespace GUI
} // namespace Ingen

#endif // LOADPATCHWINDOW_H
