///  Texturesets are used to load texture-stacks asynchronously
///
///  Author: Malte "a_teammate" Haase
///  Date:   31.12.2014
///  cJSON (Copyright (c) 2009 by Dave Gamble) is licensed under the MIT-license

///// WIP FLAG WIP FLAG
///// WIP
//// WIP
///// WIP
///// Just used to sync atm, with all stuff thrown in just so its here

#include "engine.h"
#include "texture.h"

namespace inexor {

    static VSlot *reassignvslot(Slot &owner, VSlot *vs)
    {
        owner.variants = vs;
        while(vs)
        {
            vs->slot = &owner;
            vs->linked = false;
            vs = vs->next;
        }
        return owner.variants;
    }

    static VSlot *emptyvslot(Slot &owner)
    {
        int offset = 0;
        loopvrev(slots) if(slots[i]->variants) { offset = slots[i]->variants->index + 1; break; }
        for(int i = offset; i < vslots.length(); i++) if(!vslots[i]->changed) return reassignvslot(owner, vslots[i]);
        return vslots.add(new VSlot(&owner, vslots.length()));
    }

    const char *jsontextures[8] = { "diffuse", "other", "decal", "normal", "glow", "spec", "depth", "envmap" };
    
    /// 
    class textureset
    {
    private:
        bool mounted = false;
        bool resolved = false;

    public:
        /// Wh
        vector<Slot *>texs;
        void addtexture(JSON *j);
        void addtexture(const char *filename);

        /// Makes this textureset the only one currently visible ingame
        void makecurrent();

        /// Add this textureset to the current texture stack
        void addtocurrent();


    };

    /// Loads a Texture into a Textureslot from a File/JSON
    void textureset::addtexture(JSON *j)
    {
        if(!j || texs.length() >= 0x10000) return;
        Slot &s = *texs.add(new Slot(texs.length()));
        s.loaded = false;
        loopi(8) //load textures
        {
            JSON *sub = j->getitem(jsontextures[i]);
            if(!sub) continue;
            char *name = sub->valuestring;
            if(!*name) continue;
            s.texmask |= 1 << i;
            Slot::Tex &st = s.sts.add();
            st.type = i;
            st.combined = -1;
            st.t = NULL;

            if(strpbrk(name, "/\\") && *sub->currentdir) copystring(st.name, makerelpath(sub->currentdir, name)); //relative path to current folder
            else copystring(st.name, name);
            path(st.name);
        }

        setslotshader(s, j->getitem("shader"));

        //other stuff:
        JSON *scale = j->getitem("scale"), *rot = j->getitem("rotation"),
            *xoff = j->getitem("xoffset"), *yoff = j->getitem("yoffset");

        VSlot &vs = *emptyvslot(s);
        vs.reset();
        vs.rotation = rot ? clamp(rot->valueint, 0, 5) : 0;
        vs.xoffset = xoff ? max(xoff->valueint, 0) : 0;
        vs.yoffset = yoff ? max(yoff->valueint, 0) : 0;
        vs.scale = scale ? scale->valuefloat : 1;
        //propagatevslot(&vs, (1 << VSLOT_NUM) - 1); todo
    }

    void textureset::addtexture(const char *filename)
    {
        if(!filename || !*filename) return;
        JSON *j = loadjson(filename);
        if(!j) {
            conoutf("could not load texture definition %s", filename); return;
        }

        addtexture(j);
        delete j;
    }

    /// Loads all textures from a "textures" child of given JSON structure
    void loadtextures(JSON *parent)
    {
        if(!parent) return;
        JSON *j = parent->getitem("textures");
        if(!j) return;

        loopi(j->numchilds())
        {
            const char *name = j->getstring(i);
            defformatstring(fn) ("%s", name);
            if(strpbrk(name, "/\\")) formatstring(fn)("%s", makerelpath(j->currentdir, name)); //relative path to current folder
           // loadtexturedefinition(fn);
        }
    }

    ////// Original Loading /////

    const struct slottex
    {
        const char *name;
        int id;
    } slottexs[] =
    {
        { "c", TEX_DIFFUSE },
        { "u", TEX_UNKNOWN },
        { "d", TEX_DECAL },
        { "n", TEX_NORMAL },
        { "g", TEX_GLOW },
        { "s", TEX_SPEC },
        { "z", TEX_DEPTH },
        { "e", TEX_ENVMAP }
    };

    int findslottex(const char *name)
    {
        loopi(sizeof(slottexs) / sizeof(slottex))
        {
            if(!strcmp(slottexs[i].name, name)) return slottexs[i].id;
        }
        return -1;
    }

    void texture(char *type, char *name, int *rot, int *xoffset, int *yoffset, float *scale)
    {
        if(slots.length() >= 0x10000) return;
        static int lastmatslot = -1;
        int tnum = findslottex(type), matslot = findmaterial(type);
        if(tnum<0) tnum = atoi(type);
        if(tnum == TEX_DIFFUSE) lastmatslot = matslot;
        else if(lastmatslot >= 0) matslot = lastmatslot;
        else if(slots.empty()) return;
        Slot &s = matslot >= 0 ? lookupmaterialslot(matslot, false) : *(tnum != TEX_DIFFUSE ? slots.last() : slots.add(new Slot(slots.length())));
        s.loaded = false;
        s.texmask |= 1 << tnum;
        if(s.sts.length() >= 8) conoutf(CON_WARN, "warning: too many textures in slot %d", slots.length() - 1);
        Slot::Tex &st = s.sts.add();
        st.type = tnum;
        st.combined = -1;
        st.t = NULL;
        if(name && strpbrk(name, "/\\")) copystring(st.name, makerelpath(getcurexecdir(), name)); //relative path to current folder
        else copystring(st.name, name);
        path(st.name);
        if(tnum == TEX_DIFFUSE)
        {
            setslotshader(s);
            VSlot &vs = matslot >= 0 ? lookupmaterialslot(matslot, false) : *emptyvslot(s);
            vs.reset();
            vs.rotation = clamp(*rot, 0, 5);
            vs.xoffset = max(*xoffset, 0);
            vs.yoffset = max(*yoffset, 0);
            vs.scale = *scale <= 0 ? 1 : *scale;
            //propagatevslot(&vs, (1 << VSLOT_NUM) - 1); todo
        }
        //conoutf(CON_WARN, "old texture loaded, should it be converted?")
    }
    COMMAND(texture, "ssiiif");



}