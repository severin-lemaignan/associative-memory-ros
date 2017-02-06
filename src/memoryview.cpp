/*
    Copyright (c) 2010-2016 Séverin Lemaignan (severin.lemaignan@plymouth.ac.uk)
    Copyright (C) 2009 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/circular_buffer.hpp>



#include "memoryview.h"

#include "macros.h"
#include "styles.h"

#include "node_relation.h"

#include "graph.h"

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

const int HISTORY_SAMPLING_RATE = 500;  // Hz
const int HISTORY_LENGTH = 1000;  //samples

map<size_t, boost::circular_buffer<double>> activations_logs;

microseconds last_activations_log;

void logging(microseconds time_from_start, const MemoryVector &levels) {

    // at first call, initialize the circular buffers and set their capacity.
    if (activations_logs.size() != levels.size()) {
        for (size_t i=0; i < levels.size(); i++) {
            activations_logs[i].set_capacity(HISTORY_LENGTH);
        }
    }

    // if necessary, store the activation level
    microseconds us_since_last_log = time_from_start - last_activations_log;
    if (us_since_last_log.count() > (std::micro::den * 1. / HISTORY_SAMPLING_RATE)) {
        last_activations_log = time_from_start;
        for (size_t i = 0; i < levels.size(); i++) {
            activations_logs[i].push_back(levels[i]);
        }
    }
}

MemoryView::MemoryView(const Json::Value& config, 
                       double decay_rate, double learning_rate):
    config(config),
    memory(logging, nullptr, decay_rate, learning_rate),
    display_shadows(config.get("shadows", true).asBool()),
    display_labels(config.get("display_labels", true).asBool()),
    display_footer(config.get("display_footer", false).asBool()),
    rate(30) //Hz
{


    time_scale = 1.0f;
    runtime = 0.0f;
    framecount = 0;

    //min phsyics rate 60fps (ie maximum allowed delta 1.0/60)
    max_tick_rate = 1.0 / 60.0;

    currtime = time(NULL);

    srand(currtime);

    //Nodes

    hoverNode = nullptr;

    //Mouse

    mousemoved = false;
    mouseleftclicked = false;
    mouserightclicked = false;
    mousedragged = false;

    panning = false;

    draw_loading = true;
    display_node_infos = true;
    debug = false;
    advanced_debug = false;
    paused = false;

    fontlarge = fontmanager.grab("Aller_Bd.ttf", LARGE_FONT_SIZE);
    fontlarge.dropShadow(true);
    fontlarge.roundCoordinates(true);

    fontmedium = fontmanager.grab("Aller_Lt.ttf", MEDIUM_FONT_SIZE);
    fontmedium.dropShadow(true);
    fontmedium.roundCoordinates(false);

    font = fontmanager.grab("Aller_Lt.ttf", BASE_FONT_SIZE);
    font.dropShadow(true);
    font.roundCoordinates(true);

    camera = ZoomCamera(vec3f(0,0, -300), vec3f(0.0, 0.0, 0.0), 250.0, 5000.0);

#ifndef TEXT_ONLY
    bloomtex = texturemanager.grab("bloom.tga");
    beamtex  = texturemanager.grab("beam.png");
#endif



    stylesSetup(config);
    physicsSetup(config);

    background_colour = BACKGROUND_COLOUR.truncate();
}

void MemoryView::stylesSetup(const Json::Value& config) {

    Json::Value colors = config["colours"];

    if (colors == Json::nullValue) return; // Uses defaults, as specified in styles.h

    cout << "Setting customs colors from config file." << endl;
    if (colors["selected"] != Json::nullValue) {
        ACTIVE_COLOUR = convertRGBA2Float(colors["selected"]);
    }

    if (colors["hovered"] != Json::nullValue)
        HOVERED_COLOUR = convertRGBA2Float(colors["hovered"]);

    if (colors["instances"] != Json::nullValue)
        UNITS_COLOUR = convertRGBA2Float(colors["instances"]);

    if (colors["background"] != Json::nullValue){
        BACKGROUND_COLOUR = convertRGBA2Float(colors["background"]);
    }


}

void MemoryView::physicsSetup(const Json::Value& config) {

    Json::Value physics = config["physics"];

    if (physics == Json::nullValue) return; // Uses defaults, as specified in constants.h

    cout << "Setting customs physics parameters from config file." << endl;
    if (physics["mass"] != Json::nullValue) {
        INITIAL_MASS = physics["mass"].asDouble();
    }
    if (physics["damping"] != Json::nullValue) {
        INITIAL_DAMPING = physics["damping"].asDouble();
    }
    if (physics["repulsion"] != Json::nullValue) {
        COULOMB_CONSTANT = physics["repulsion"].asDouble();
    }
    if (physics["maxspeed"] != Json::nullValue) {
        MAX_SPEED = physics["maxspeed"].asDouble();
    }


}

vec4f MemoryView::convertRGBA2Float(const Json::Value& color) {
    return vec4f(color[0u].asInt()/255.0,
                 color[1u].asInt()/255.0,
                 color[2u].asInt()/255.0,
                 color[3u].asInt()/255.0);
}

/** Initialization */
void MemoryView::init(){


    cerr << "Memory network initialization" << endl;

    memory.record(true);
    memory.start();

    cerr << "Subscribing to attentional targets" << endl;

    attention_targets = nh.subscribe("attention_targets", 1, &MemoryView::on_attention_target, this);

    cerr << "Associative memory network up and running" << endl;
}

void MemoryView::on_attention_target(const playground_builder::AttentionTargetsStamped::ConstPtr& msg) {

    // activate unit corresponding to current user
    if (!memory.has_unit(msg->header.frame_id)) memory.add_unit(msg->header.frame_id);
    memory.activate_unit(msg->header.frame_id, 1.0, chrono::milliseconds(60));


    for (const auto& target : msg->targets) {
        if (!memory.has_unit(target.frame_id)) memory.add_unit(target.frame_id);
        memory.activate_unit(target.frame_id, target.intensity, chrono::milliseconds(60));
    }



}


/** Events */
void MemoryView::keyPress(SDL_KeyboardEvent *e) {
    if (e->type == SDL_KEYUP) return;

    if (e->type == SDL_KEYDOWN) {
        if (e->keysym.sym == SDLK_ESCAPE) {
            memory.stop();
            memory.save_record();
            appFinished=true;
        }

        if (e->keysym.sym == SDLK_d) {
            if (debug)
                if (advanced_debug) {
                    debug = false;
                    advanced_debug = false;
                }
                else advanced_debug = true;
            else debug = true;
        }
        //        if (e->keysym.sym == SDLK_w) {
        //            gGourceQuadTreeDebug = !gGourceQuadTreeDebug;
        //        }

        if (e->keysym.sym == SDLK_TAB) {
            _activate_on_hover = !_activate_on_hover;
        }

        if (e->keysym.sym == SDLK_SPACE) {
            //addRandomNodes(2, 2);
            memory.max_frequency(20);
        }

        if (e->keysym.sym == SDLK_p) {
            paused = !paused;
        }

        if(e->keysym.sym == SDLK_UP) {
            zoom(true);
        }

        if(e->keysym.sym == SDLK_DOWN) {
            zoom(false);
        }

        if(e->keysym.sym == SDLK_s) {
            g.saveToGraphViz(*this);
        }

        if(e->keysym.sym == SDLK_a) {
            memory.add_unit(string("input") + to_string(memory.size()));
        }
    }
}

void MemoryView::mouseClick(SDL_MouseButtonEvent *e) {

    if(e->type == SDL_MOUSEBUTTONUP) {

        if(e->button == SDL_BUTTON_LEFT || e->button == SDL_BUTTON_MIDDLE) {

            mouse_inactivity=0.0;
            SDL_ShowCursor(true);

            SDL_WM_GrabInput(SDL_GRAB_OFF);

            //stop dragging mouse, return the mouse to where
            //the user started dragging.
            if(mousedragged) {
                SDL_WarpMouse(mousepos.x, mousepos.y);
                mousedragged=false;
            }
            panning = false;
            draggedNode = nullptr;
        }
    }

    if(e->type != SDL_MOUSEBUTTONDOWN) return;

    switch(e->button) {

    case SDL_BUTTON_WHEELUP:
        zoom(true);
        break;

    case SDL_BUTTON_WHEELDOWN:
        zoom(false);
        break;

    case SDL_BUTTON_RIGHT:
        mousepos = vec2f(e->x, e->y);
        mouserightclicked=true;
        break;

    case SDL_BUTTON_LEFT:
        mousepos = vec2f(e->x, e->y);
        mouseleftclicked=true;
        mousedragged=true;
        if (hoverNode) draggedNode = hoverNode;
        break;

    case SDL_BUTTON_MIDDLE:
        mousepos = vec2f(e->x, e->y);
        panning=true;
        mousedragged=true;
        break;
    }

}

void MemoryView::mouseMove(SDL_MouseMotionEvent *e) {

    mousepos = vec2f(e->x, e->y);

    if(mousedragged) {
        if (panning) {
            //move camera in direction the user dragged the mouse
            backgroundPos -= vec2f( e->xrel, e->yrel );
        }
        else {
            if (draggedNode) {
                draggedNode->pos += vec2f( e->xrel, e->yrel )/2;
            }
         }

        return;
    }

    mouse_inactivity = 0.0;
    SDL_ShowCursor(true);

    mousemoved=true;
}

/** main update function */
void MemoryView::update(float t, float dt) {

    SDL_Delay(20); //TODO Not too quick...

    dt = min(dt, max_tick_rate);

    dt *= time_scale;

    //have to manage runtime internally as we're messing with dt
    runtime += dt;

    logic_time = SDL_GetTicks();

    logic(runtime, dt);

    logic_time = SDL_GetTicks() - logic_time;

    draw_time = SDL_GetTicks();

    draw(runtime, dt);

    framecount++;


    ros::spinOnce();
}

/** App logic */
void MemoryView::logic(float t, float dt) {
    if(draw_loading && logic_time > 1000) draw_loading = false;

    //still want to update camera while paused
    if(paused) {
        updateCamera(dt);
        return;
    }

    //check if mouse has been inactive for 3 seconds
    //and if so hide it.
    if(!mouseleftclicked && !mouserightclicked && mouse_inactivity<3.0) {
        mouse_inactivity += dt;

        if(mouse_inactivity>=3.0) {
            SDL_ShowCursor(false);
        }
    }

    // Activate units under the mouse
    if(hoverNode && _activate_on_hover) {
        memory.activate_unit(hoverNode->getID(), 1.0, 40000us);
    }

    updateFromMemoryNetwork(memory);
    g.step(dt);

    updateCamera(dt);
}

void MemoryView::mouseTrace(Frustum& frustum, float dt) {

    GLuint	buffer[512];
    GLint	viewport[4];

    glGetIntegerv(GL_VIEWPORT, viewport);
    glSelectBuffer(512, buffer);

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    /** Start the selection of primitives touched by the mouse **/
    (void) glRenderMode(GL_SELECT);

    glInitNames();
    glPushName(0);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    //Restrict the detection to a zone around of the mouse
    gluPickMatrix((GLdouble) mousepos.x, (GLdouble) (viewport[3]-mousepos.y), 1.0f, 1.0f, viewport);
    gluPerspective(90.0f, (GLfloat)display.width/(GLfloat)display.height, 0.1f, camera.getZFar());
    camera.look();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0, 1.0, 1.0, 1.0);

    g.render(SIMPLE, *this);

    glMatrixMode(GL_MODELVIEW);

    //going back to the GL_RENDER mode, returning the number of hits from hte GL_SELECT mode
    mouse_hits = glRenderMode(GL_RENDER);
    /** End of selection **/

    Node* nodeSelection = nullptr;

    if (mouse_hits > 0) {

        int choice   = buffer[3];
        GLuint depth = buffer[1];

        for (int loop = 1; loop < mouse_hits; loop++) {
            if (buffer[loop*4+1] < depth) {
                choice = buffer[loop*4+3];
                depth  = buffer[loop*4+1];
            }
        }

        if(choice != 0) {
            selectionDepth = depth;
            nodeSelection = &g.getNode(choice - 1); // cf node_rendered for the rationale of the -1 (basically, to make sure we can select the node 0)
        }
    }

    glDisable(GL_DEPTH_TEST);

    // is over a file
    if(nodeSelection) {

        if(nodeSelection != hoverNode) {
            //deselect previous selection
            if(hoverNode) hoverNode->hovered(false);

            //select new
            nodeSelection->hovered(true);
            hoverNode = nodeSelection;
        }
    }
    else {
        if(hoverNode) hoverNode->hovered(false);
        hoverNode=nullptr;
    }

    if(mouseleftclicked) {
        if(hoverNode) {
            if (hoverNode->selected()) {
                g.deselect(hoverNode);
            }
            else {
                addSelectedNode(hoverNode);
            }
        }
        else {
            g.clearSelect();
        }
    }

    if(mouserightclicked) {
            g.clearSelect();
    }
}

void MemoryView::drawNodeDetails(Node* node, int offset, bool highlight) {

        
        if (highlight)
            glColor4f(0.f, 0.f, 0.f, 1.f);
        else
            glColor4f(.5f, .5f, .5f, .5f);

        fontmedium.print(10,offset,"Node %s:", node->label.c_str());
        fontmedium.print(40, offset + 20,"Activity: %.4f", node->activity);


        // Display a graph of the unit's recent activity
        
        const int v_offset = offset + 50; // px
        const int h_offset = 30; // px
        const double height = 50; //px
        const double width = 150; //px


        glColor4f(0.f, .1f, 0.1f, .6f);
        font.print(10, v_offset - 5 ,"1.0");
        font.print(15, v_offset + height - 5,"0");
        font.print(5, v_offset + height - height * memory.Amin - 5 ,"-0.2");

        glBindTexture(GL_TEXTURE_2D, beamtex->textureid);

        glDisable(GL_TEXTURE_2D);
        // graph bounding box
        glBegin(GL_LINE_STRIP);
            glVertex2f(h_offset, v_offset);
            glVertex2f(h_offset + width, v_offset);
            glVertex2f(h_offset + width, v_offset + height - memory.Amin * height);
            glVertex2f(h_offset, v_offset + height - memory.Amin * height);
            glVertex2f(h_offset, v_offset);
        glEnd();

        // line '0'
        glColor4f(0.f, 1.f, 0.2f, .6f);
        glBegin(GL_LINE_STRIP);
            glVertex2f(h_offset, v_offset + height);
            glVertex2f(h_offset + width, v_offset + height);
        glEnd();

        // graph itself
        glColor4f(1.f, .2f, 0.2f, 1.f);
        auto history = activations_logs[node->getID()];

        glBegin(GL_LINE_STRIP);
        for(int i=0;i<history.size();i++) {
            auto activity = history[i];

            vec2f pos1(h_offset + i * width / history.size(), v_offset + height - activity * height);
            vec2f pos2(h_offset + (i+1) * width / history.size(), v_offset + height - activity * height);

            glVertex2fv(pos1);
            glVertex2fv(pos2);
        }

        glEnd();

        glEnable(GL_TEXTURE_2D);

}

/** Drawing */
void MemoryView::draw(float t, float dt) {

    auto selectedNodes = g.getAllSelected();

#ifndef TEXT_ONLY

    display.mode2D();

    drawBackground(dt);

    Frustum frustum(camera);

    trace_time = SDL_GetTicks();

    mouseTrace(frustum,dt);

    trace_time = SDL_GetTicks() - trace_time;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    camera.focus();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

#endif
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    //Draw shadows for edges and then nodes
    if (display_shadows) g.render(SHADOWS, *this, advanced_debug);

    //Draw edges and then nodes
    glBindTexture(GL_TEXTURE_2D, beamtex->textureid);
    g.render(NORMAL, *this, advanced_debug);

    //draw 'gourceian blur' around dirnodes
    glBlendFunc (GL_ONE, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, bloomtex->textureid);
    //Draw Bloom
    g.render(BLOOM, *this, advanced_debug);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Draw names
    if (display_labels) g.render(NAMES, *this, advanced_debug);

#ifndef TEXT_ONLY



    if(advanced_debug) {

        glDisable(GL_TEXTURE_2D);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        nodesBounds.draw();

    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    display.mode2D();

    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    //fontmedium.draw(display.width/2 - date_x_offset, 20, displaydate);

    glColor4f(.5f, .5f, .5f, .5f);
    auto freq = memory.frequency();
    if (freq > 2000)
        fontmedium.print(10,20, "Network update frequency: %dkHz", memory.frequency()/1000);
    else
        fontmedium.print(10,20, "Network update frequency: %dHz", memory.frequency());
    fontmedium.print(10,50, "Mode (tab to switch): %s",
                            _activate_on_hover ? "EXCITATE" : "INSPECT");

    int display_offset = 80;
    if(hoverNode) {
        drawNodeDetails(hoverNode, display_offset, true);
    }
    display_offset += 120;

    for (auto node : selectedNodes) {
        drawNodeDetails(node, display_offset, false);
        display_offset += 120;
    }

    int offset = 200;
    if(debug) {
        vec3f campos = camera.getPos();

        glDisable(GL_TEXTURE_2D);
        glColor4f(.5f, .5f, .5f, .5f);

        nodesBounds.draw();

        font.print(10,offset + 20, "FPS: %.2f", fps);
        font.print(10,offset + 40,"Time Scale: %.2f", time_scale);
        font.print(10,offset + 80,"Nodes: %d", g.nodesCount());
        font.print(10,offset + 100,"Edges: %d", g.edgesCount());

        font.print(10,offset + 140,"Camera: (%.2f, %.2f, %.2f)", campos.x, campos.y, campos.z);
        font.print(10,offset + 160,"Gravity: %.2f", GRAVITY);
        font.print(10,offset + 180,"Logic Time: %u ms", logic_time);
        font.print(10,offset + 200,"Mouse Trace: %u ms", trace_time);
        font.print(10,offset + 220,"Draw Time: %u ms", SDL_GetTicks() - draw_time);

        if(hoverNode) {
            font.print(10,offset + 260,"Node %s:", hoverNode->label.c_str());
            font.print(40,offset + 280,"Speed: (%.2f, %.2f)", hoverNode->speed.x, hoverNode->speed.y);
            font.print(40,offset + 300,"Charge: %.2f", hoverNode->charge);
            font.print(40,offset + 320,"Kinetic energy: %.2f", hoverNode->kinetic_energy);
        }

    }

    if (display_footer) drawFooter();

    glDisable(GL_TEXTURE_2D);


    mousemoved=false;
    mouseleftclicked=false;
    mouserightclicked=false;
#endif

}

void MemoryView::drawBackground(float dt) {
    display.setClearColour(background_colour);
    display.clear();
}

void MemoryView::queueNodeInFooter(int id) {
    queueInFooter(g.getNode(id).renderer.getLabel());
}
void MemoryView::queueInFooter(const string& text) {

    int max_x = display.width + 10;

    // Ensure we do not overlap with previous text
    typedef pair<const string, int> mypair;
    for(auto elem : footer_content) {
        int x = elem.second + fontlarge.getWidth(elem.first);
        max_x = max(x + 50, max_x);
    }

    footer_content[text] = max_x;
}

void MemoryView::drawFooter() {

    glColor4f(1.0f, 1.0f, 0.8f, 0.7f);

    for( map<string, int>::iterator iter = footer_content.begin() ;
         iter != footer_content.end() ;
         ) {

        if (iter->second < -fontlarge.getWidth(iter->first)) {
            map<string, int>::iterator elem_to_remove = iter++;
            footer_content.erase(elem_to_remove);
            continue;
        }

        fontlarge.print(iter->second,display.height - fontlarge.getFontSize() - 20, "%s", iter->first.c_str());

        iter->second -= FOOTER_SPEED;

        iter++;
    }

}

void MemoryView::loadingScreen() {
    display.mode2D();

    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    glColor4f(1.0, 1.0, 1.0, 1.0);

    string loading_message("Initializing Memory View...");
    int width = font.getWidth(loading_message);

    font.print(display.width/2 - width/2, display.height/2 - 10, "%s", loading_message.c_str());
}

void MemoryView::drawVector(vec2f vec, vec2f pos, vec4f col) {

    float radius = 5.0;

    //conversion not optimal
    float angle = atan2(vec.y, vec.x) * 180 / 3.1415926;

    glPushMatrix();
    glTranslatef(pos.x, pos.y, 0.0f);
    glRotatef(angle, 0.0, 0.0, 1.0);

    glColor4fv(col);

    glBegin(GL_QUADS);

    // src point

    glTexCoord2f(0.0,0.0);
    glVertex2f(0, -radius/3);
    glTexCoord2f(1.0,0.0);
    glVertex2f(0, radius/3);

    // dest point
    glTexCoord2f(1.0,0.0);
    glVertex2f(vec.length() - 5.0, radius/3);
    glTexCoord2f(0.0,0.0);
    glVertex2f(vec.length() - 5.0, -radius/3);


    glEnd();

    //Arrow
    glBegin(GL_TRIANGLES);

    glTexCoord2f(0.0,0.0);
    glVertex2f(vec.length() - radius, -radius/2);
    glTexCoord2f(0.5,1.0);
    glVertex2f(vec.length(), 0);
    glTexCoord2f(1.0,0.0);
    glVertex2f(vec.length() - radius, radius/2);

    glEnd();

    glPopMatrix();
}

void MemoryView::displayCoulombField() {
    for (float i = -200.0; i < 200.0; i += 40.0) {
        for (float j = -200.0; j < 200.0; j += 40.0) {
            vec2f pos(i, j);

            drawVector(g.coulombRepulsionAt(pos), pos, (1.0, 1.0, 0.2, 0.7));
        }
    }
}
/** Camera */
void MemoryView::updateCamera(float dt) {

    //camera tracking

    Bounds2D mousebounds;
    mousebounds.update(backgroundPos);

    camera.adjust(mousebounds);

    camera.logic(dt);
}

void MemoryView::zoom(bool zoomin) {

    float min_distance = camera.getMinDistance();
    float max_distance = camera.getMaxDistance();

    float zoom_multi = 1.1;

    if(zoomin) {
        min_distance /= zoom_multi;
        if(min_distance < 100.0) min_distance = 100.0;

        camera.setMinDistance(min_distance);
    } else {
        min_distance *= zoom_multi;
        if(min_distance > 1000.0) min_distance = 1000.0;

        camera.setMinDistance(min_distance);
    }
}

/** Background */
void MemoryView::setBackground(vec3f background) {
    background_colour = background;
}

//trace click of mouse on background
void MemoryView::selectBackground() {

    g.clearSelect();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    gluPerspective(90.0f, (GLfloat)display.width/(GLfloat)display.height, 0.1f, -camera.getPos().z);
    camera.look();

    vec2f screen_centre(display.width*0.5,display.height*0.5);

    backgroundPos = display.unproject(screen_centre).truncate();

    SDL_ShowCursor(false);
    SDL_WM_GrabInput(SDL_GRAB_ON);

}

/** Nodes */
//select a node, deselect current node
void MemoryView::selectNode(Node* node) {

    if (node->selected()) return;

    g.clearSelect();
    g.select(node);

    queueNodeInFooter(node->getID());
}

//select a node, keep currently selected node
void MemoryView::addSelectedNode(Node* node) {

    if (node->selected()) g.deselect(node);
    else {
        g.select(node);
        queueNodeInFooter(node->getID());
    }
}

void MemoryView::initFromMemoryNetwork() {


    for (size_t i = 0; i < memory.size(); i++) {
        Node& n = g.addNode(i, memory.units_names()[i]);
    }

    for (size_t i = 0; i < memory.size()-1; i++) {
        for (size_t j = i+1; j < memory.size(); j++) {

            auto& n1 = g.getNode(i);
            auto& n2 = g.getNode(j);
            if(!g.getEdge(n1, n2)) g.addEdge(n1, n2);
        }
    }

}

void MemoryView::updateFromMemoryNetwork(const MemoryNetwork& memory) {

    if (memory.size() > g.nodesCount()) initFromMemoryNetwork();

    for (size_t i = 0; i < memory.size(); i++) {
        g.getNode(i).activity = memory.activations()(i);
    }

    for (auto& edge : *g.getEdges()) {
        edge.setWeight(memory.weights()(edge.getId1(),edge.getId2()));
    }


}

Node& MemoryView::getNode(int id) {
    return g.getNode(id);
}

/** Testing */
void MemoryView::addRandomNodes(int amount,int nb_rel) {

    const int length = 6; //length of randomly created ID.

    for (int i = 0; i < amount ; ++i) {

        string newId;

        //Generate a new random id for this node
        for(int j=0; j<length; ++j)
        {
            newId += (char)(rand() % 26 + 97); //ASCII codes of letters starts at 98 for "a"
        }

        auto neighbour = g.getRandomNode();

        Node& n = g.addNode(g.getNodes().size(), newId, neighbour);
        vec4f col((float)rand()/RAND_MAX, (float)rand()/RAND_MAX, (float)rand()/RAND_MAX, 0.7);
        n.setColour(col);

        if (neighbour)
            g.addEdge(n, *neighbour);

        for(int k=0; k<(nb_rel - 1); ++k) {

            //We may pick ourselves, but it's not that a problem
            auto n2 = g.getRandomNode();
            g.addEdge(n, *n2);
        }
    }
}
