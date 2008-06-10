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

#include <cassert>
#include <raul/Atom.hpp>
#include "interface/EngineInterface.hpp"
#include "client/NodeModel.hpp"
#include "client/PatchModel.hpp"
#include "client/PluginUI.hpp"
#include "App.hpp"
#include "GladeFactory.hpp"
#include "NodeControlWindow.hpp"
#include "NodeModule.hpp"
#include "PatchCanvas.hpp"
#include "PatchWindow.hpp"
#include "Port.hpp"
#include "RenameWindow.hpp"
#include "SubpatchModule.hpp"
#include "WindowFactory.hpp"

namespace Ingen {
namespace GUI {


NodeModule::NodeModule(boost::shared_ptr<PatchCanvas> canvas, SharedPtr<NodeModel> node)
	: FlowCanvas::Module(canvas, node->path().name())
	, _node(node)
	, _gui_widget(NULL)
	, _gui_container(NULL)
	, _gui_item(NULL)
	, _gui_window(NULL)
	, _last_gui_request_width(0)
	, _last_gui_request_height(0)
{
	assert(_node);

	node->signal_new_port.connect(sigc::bind(sigc::mem_fun(this, &NodeModule::add_port), true));
	node->signal_removed_port.connect(sigc::mem_fun(this, &NodeModule::remove_port));
	node->signal_variable.connect(sigc::mem_fun(this, &NodeModule::set_variable));
	node->signal_polyphonic.connect(sigc::mem_fun(this, &NodeModule::set_stacked_border));
	node->signal_renamed.connect(sigc::mem_fun(this, &NodeModule::rename));
	
	set_stacked_border(node->polyphonic());
}


NodeModule::~NodeModule()
{
	NodeControlWindow* win = App::instance().window_factory()->control_window(_node);
	
	if (win) {
		// Should remove from window factory via signal
		delete win;
	}
}


void
NodeModule::create_menu()
{
	Glib::RefPtr<Gnome::Glade::Xml> xml = GladeFactory::new_glade_reference();
	xml->get_widget_derived("object_menu", _menu);
	_menu->init(_node);
	_menu->signal_embed_gui.connect(sigc::mem_fun(this, &NodeModule::embed_gui));
	_menu->signal_popup_gui.connect(sigc::hide_return(sigc::mem_fun(this, &NodeModule::popup_gui)));
	
	set_menu(_menu);
}


boost::shared_ptr<NodeModule>
NodeModule::create(boost::shared_ptr<PatchCanvas> canvas, SharedPtr<NodeModel> node)
{
	boost::shared_ptr<NodeModule> ret;

	SharedPtr<PatchModel> patch = PtrCast<PatchModel>(node);
	if (patch)
		ret = boost::shared_ptr<NodeModule>(new SubpatchModule(canvas, patch));
	else
		ret = boost::shared_ptr<NodeModule>(new NodeModule(canvas, node));

	for (GraphObject::Variables::const_iterator m = node->variables().begin(); m != node->variables().end(); ++m)
		ret->set_variable(m->first, m->second);

	for (PortModelList::const_iterator p = node->ports().begin(); p != node->ports().end(); ++p) {
		ret->add_port(*p, false);
	}

	ret->resize();

	return ret;
}

	
void
NodeModule::value_changed(uint32_t index, const Atom& value)
{
	float control = value.get_float();
	if (_plugin_ui) {
		SLV2UIInstance inst = _plugin_ui->instance();
		const LV2UI_Descriptor* ui_descriptor = slv2_ui_instance_get_descriptor(inst);
		LV2UI_Handle ui_handle = slv2_ui_instance_get_handle(inst);
		if (ui_descriptor->port_event)
		  ui_descriptor->port_event(ui_handle, index, 4, 0, &control);
	}
}


void
NodeModule::embed_gui(bool embed)
{
	if (embed) {

		if (_gui_window) {
			cerr << "LV2 GUI already popped up, cannot embed" << endl;
			return;
		}
		
		GtkWidget* c_widget = NULL;

		if (!_gui_item) {
		
			const PluginModel* const plugin = dynamic_cast<const PluginModel*>(_node->plugin());
			assert(plugin);

			_plugin_ui = plugin->ui(App::instance().world(), _node);

			if (_plugin_ui) {
				c_widget = (GtkWidget*)slv2_ui_instance_get_widget(_plugin_ui->instance());
				_gui_widget = Glib::wrap(c_widget);
				assert(_gui_widget);
			
				if (_gui_container)
					delete _gui_container;

				_gui_container = manage(new Gtk::EventBox());
				_gui_container->set_name("ingen_embedded_node_gui_container");
				_gui_container->set_border_width(2);
				_gui_container->add(*_gui_widget);
				_gui_container->show_all();
			
				const double y = 4 + _canvas_title.property_text_height();
				_gui_item = new Gnome::Canvas::Widget(*this, 2.0, y, *_gui_container);
			}
		}

		if (_gui_item) {

			assert(_gui_widget);
			_gui_widget->show_all();
			_gui_item->show();

			Gtk::Requisition r = _gui_container->size_request();
			gui_size_request(&r, true);

			_gui_item->raise_to_top();

			_gui_container->signal_size_request().connect(sigc::bind(
					sigc::mem_fun(this, &NodeModule::gui_size_request), false));
		
			for (PortModelList::const_iterator p = _node->ports().begin(); p != _node->ports().end(); ++p)
				if ((*p)->type().is_control() && (*p)->is_output())
					App::instance().engine()->enable_port_broadcasting((*p)->path());

		} else {
			cerr << "ERROR: Failed to create canvas item for LV2 UI" << endl;
		}

	} else { // un-embed

		if (_gui_item) {
			delete _gui_item;
			_gui_item = NULL;
			_gui_container = NULL; // managed
			_gui_widget = NULL; // managed
			_plugin_ui.reset();
		}

		_ports_y_offset = 0;
		_minimum_width = 0; // resize() takes care of it..

		for (PortModelList::const_iterator p = _node->ports().begin(); p != _node->ports().end(); ++p)
			if ((*p)->type().is_control() && (*p)->is_output())
				App::instance().engine()->disable_port_broadcasting((*p)->path());
	}
			
	if (embed && _gui_item) {
		initialise_gui_values();
		set_base_color(0x212222FF);
	} else {
		set_default_base_color();
	}

	resize();
}


void
NodeModule::gui_size_request(Gtk::Requisition* r, bool force)
{
	if (!force && _last_gui_request_width == r->width && _last_gui_request_height == r->height)
		return;

	if (r->width + 4 > _width)
		set_minimum_width(r->width + 4);
	
	_ports_y_offset = r->height + 2;
	
	resize();

	Gtk::Allocation allocation;
	allocation.set_width(r->width + 4);
	allocation.set_height(r->height + 4);

	_gui_container->size_allocate(allocation);
	_gui_item->property_width() = _width - 4;
	_gui_item->property_height() = r->height;

	_last_gui_request_width = r->width;
	_last_gui_request_height = r->height;
}


void
NodeModule::rename()
{
	set_name(_node->path().name());
	resize();
}


void
NodeModule::add_port(SharedPtr<PortModel> port, bool resize_to_fit)
{
	uint32_t index = _ports.size(); // FIXME: kludge, engine needs to tell us this
	
	Module::add_port(boost::shared_ptr<Port>(
			new Port(PtrCast<NodeModule>(shared_from_this()), port)));
		
	port->signal_value_changed.connect(sigc::bind<0>(
			sigc::mem_fun(this, &NodeModule::value_changed), index));

	if (resize_to_fit)
		resize();
}


void
NodeModule::remove_port(SharedPtr<PortModel> port)
{
	SharedPtr<FlowCanvas::Port> p = Module::remove_port(port->path().name());
	p.reset();
}


bool
NodeModule::popup_gui()
{
#ifdef HAVE_SLV2
	if (_node->plugin()->type() == PluginModel::LV2) {
		if (_plugin_ui) {
			cerr << "LV2 GUI already embedded, cannot pop up" << endl;
			return false;
		}

		const PluginModel* const plugin = dynamic_cast<const PluginModel*>(_node->plugin());
		assert(plugin);

		_plugin_ui = plugin->ui(App::instance().world(), _node);

		if (_plugin_ui) {
			GtkWidget* c_widget = (GtkWidget*)slv2_ui_instance_get_widget(_plugin_ui->instance());
			_gui_widget = Glib::wrap(c_widget);
			
			_gui_window = new Gtk::Window();
			_gui_window->add(*_gui_widget);
			_gui_widget->show_all();
			initialise_gui_values();

			_gui_window->signal_unmap().connect(
					sigc::mem_fun(this, &NodeModule::on_gui_window_close));
			_gui_window->present();

			return true;
		} else {
			cerr << "No LV2 GUI" << endl;
		}
	}
#endif
	return false;
}


void
NodeModule::on_gui_window_close()
{
	delete _gui_window;
	_gui_window = NULL;
	_plugin_ui.reset();
	_gui_widget = NULL;
}
	

void
NodeModule::initialise_gui_values()
{
	uint32_t index=0;
	for (PortModelList::const_iterator p = _node->ports().begin(); p != _node->ports().end(); ++p) {
		if ((*p)->type().is_control())
			value_changed(index, (*p)->value());
		++index;
	}
}


void
NodeModule::show_control_window()
{
	App::instance().window_factory()->present_controls(_node);
}
	

void
NodeModule::on_double_click(GdkEventButton* ev)
{
	if ( ! popup_gui() )
		show_control_window();
}


void
NodeModule::store_location()
{
	const float x = static_cast<float>(property_x());
	const float y = static_cast<float>(property_y());
	
	const Atom& existing_x = _node->get_variable("ingenuity:canvas-x");
	const Atom& existing_y = _node->get_variable("ingenuity:canvas-y");
	
	if (existing_x.type() != Atom::FLOAT || existing_y.type() != Atom::FLOAT
			|| existing_x.get_float() != x || existing_y.get_float() != y) {
		App::instance().engine()->set_variable(_node->path(), "ingenuity:canvas-x", Atom(x));
		App::instance().engine()->set_variable(_node->path(), "ingenuity:canvas-y", Atom(y));
	}
}


void
NodeModule::set_variable(const string& key, const Atom& value)
{
	if (key == "ingenuity:canvas-x" && value.type() == Atom::FLOAT)
		move_to(value.get_float(), property_y());
	else if (key == "ingenuity:canvas-y" && value.type() == Atom::FLOAT)
		move_to(property_x(), value.get_float());
}


} // namespace GUI
} // namespace Ingen
