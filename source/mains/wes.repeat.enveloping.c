/**
@file
wes.repeat.enveloping.c

@name
wes.repeat.enveloping~

@realname
wes.repeat.enveloping~

@type
object

@module
waveset

@author
Marco Marasciuolo

@digest
 Apply envelope

@description
Repeats each wavesets and apply an amplitude or pitch envelope.

@discussion

@category
waveset basic

@keywords
buffer, waveset

@seealso
wes.repeat.overlap~ , wes.repeat.attract~ , wes.repeat.pendulum~, wes.repeat.simplify~

@owner
Marco Marasciuolo
*/
#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"





typedef struct _buf_repeatgliss {
    t_earsbufobj        e_ob;
    long sampMin_in;
    long cross_in;
    long repeatMult_in;
    float slopePitch_in;
    float slopeAmp_in;
    int pitchEGtype_in;
    int ampEGtype_in;
    int envPitchOnOff_in;
    int envAmpOnOff_in;
    float pitchMin_in;
    float pitchMax_in;
    t_llll  *envin;

    
} t_buf_repeatgliss;




// Prototypes
t_buf_repeatgliss*         buf_repeatgliss_new(t_symbol *s, short argc, t_atom *argv);
void            buf_repeatgliss_free(t_buf_repeatgliss *x);
void            buf_repeatgliss_bang(t_buf_repeatgliss *x);
void            buf_repeatgliss_anything(t_buf_repeatgliss *x, t_symbol *msg, long ac, t_atom *av);

void buf_repeatgliss_assist(t_buf_repeatgliss *x, void *b, long m, long a, char *s);
void buf_repeatgliss_inletinfo(t_buf_repeatgliss *x, void *b, long a, char *t);

void repeatgliss_bang(t_buf_repeatgliss *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType);

// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(repeatgliss)

/**********************************************************************/
// Class Definition and Life Cycle

void C74_EXPORT ext_main(void* moduleRef)
{
    common_symbols_init();
    llllobj_common_symbols_init();
    
    if (llllobj_check_version(bach_get_current_llll_version()) || llllobj_test()) {
        ears_error_bachcheck();
        return;
    }
    
    t_class *c;
    
    CLASS_NEW_CHECK_SIZE(c, "wes.repeat.enveloping~",
                         (method)buf_repeatgliss_new,
                         (method)buf_repeatgliss_free,
                         sizeof(t_buf_repeatgliss),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
    // @method symbol @digest Process buffers
    // @description A symbol with buffer name will trigger the buffer processing and output the processed
    // buffer name (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(repeatgliss)
    
    CLASS_ATTR_LONG(c, "minsamp", 0, t_buf_repeatgliss, sampMin_in);
    CLASS_ATTR_LONG(c, "cross", 0, t_buf_repeatgliss, cross_in);
    CLASS_ATTR_LONG(c, "repeatmult", 0, t_buf_repeatgliss, repeatMult_in);
    CLASS_ATTR_FLOAT(c, "envpitchslope", 0, t_buf_repeatgliss, slopePitch_in);
    CLASS_ATTR_FLOAT(c, "envampslope", 0, t_buf_repeatgliss, slopeAmp_in);
    CLASS_ATTR_FLOAT(c, "pitchmin", 0, t_buf_repeatgliss, pitchMin_in);
    CLASS_ATTR_FLOAT(c, "pitchmax", 0, t_buf_repeatgliss, pitchMax_in);
    
    CLASS_ATTR_CHAR(c, "envelopepitchtype", 0, t_buf_repeatgliss, pitchEGtype_in);
      CLASS_ATTR_STYLE_LABEL(c,"envelopepitchtype",0,"enumindex","Envelope Pitch Type");
      CLASS_ATTR_ENUMINDEX(c,"envelopepitchtype", 0, "Fall Rise");
    
    CLASS_ATTR_CHAR(c, "envelopeamptype", 0, t_buf_repeatgliss, ampEGtype_in);
    CLASS_ATTR_STYLE_LABEL(c,"envelopeamptype",0,"enumindex","Envelope Amp Type");
    CLASS_ATTR_ENUMINDEX(c,"envelopeamptype", 0, "Fall Rise");

    CLASS_ATTR_CHAR(c, "envelopepitchon", 0, t_buf_repeatgliss, envPitchOnOff_in);
       CLASS_ATTR_STYLE_LABEL(c,"envelopepitchon",0,"enumindex","Envelope Pitch On");
       CLASS_ATTR_ENUMINDEX(c,"envelopepitchon", 0, "On Off");
     
     CLASS_ATTR_CHAR(c, "envelopeampon", 0, t_buf_repeatgliss, envAmpOnOff_in);
     CLASS_ATTR_STYLE_LABEL(c,"envelopeampon",0,"enumindex","Envelope Amp On");
     CLASS_ATTR_ENUMINDEX(c,"envelopeampon", 0, "On Off");

    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_repeatgliss_assist(t_buf_repeatgliss *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        if (a == 0) // @in 0 @type symbol @digest Buffer name(s)
            sprintf(s, "symbol: Buffer Names");
        else if (a == 1) // @in 1 @type number/symbol @digest number of repeat
            sprintf(s, "Buffer/float: number of repeat"); // @number of repeat
    } else {
        sprintf(s, "New Buffer Names"); // @out 0 @type symbol/list @digest Output buffer names(s)
                                            // @description Name of the buffer
    }
}

void buf_repeatgliss_inletinfo(t_buf_repeatgliss *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_repeatgliss *buf_repeatgliss_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_repeatgliss *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_repeatgliss*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->envin = llll_from_text_buf("1", false);
        x->sampMin_in = 150;
        x->cross_in = 1;
        x->repeatMult_in = 5;
        x->slopePitch_in = 2;
        x->slopeAmp_in = 2;
        x->pitchEGtype_in = 0;
        x->ampEGtype_in = 0;
        x->envPitchOnOff_in = 0;
        x->envAmpOnOff_in = 0;
        x->pitchMin_in = 0.05;
        x->pitchMax_in = 2;
 
  
        earsbufobj_init((t_earsbufobj *)x,  0);
        
        // @arg 0 @name outnames @optional 1 @type symbol
        // @digest Output buffer names
        // @description @copy EARS_DOC_OUTNAME_ATTR
        
  
        
        t_llll *args = llll_parse(true_ac, argv);
        t_llll *names = earsbufobj_extract_names_from_args((t_earsbufobj *)x, args);
        
        if (args && args->l_head) {
            llll_clear(x->envin);
            llll_appendhatom_clone(x->envin, &args->l_head->l_hatom);
        }
 
        
        attr_args_process(x, argc, argv);
        
        earsbufobj_setup((t_earsbufobj *)x, "E4", "E", names);
        
        llll_free(args);
        llll_free(names);
    }
    return x;
}


void buf_repeatgliss_free(t_buf_repeatgliss *x)
{
    llll_free(x->envin);
    earsbufobj_free((t_earsbufobj *)x);
    
}




void buf_repeatgliss_bang(t_buf_repeatgliss *x)
{
    long num_buffers = earsbufobj_get_instore_size((t_earsbufobj *)x, 0);
    earsbufobj_refresh_outlet_names((t_earsbufobj *)x);
    earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_IN, 0, num_buffers, true);
    
    earsbufobj_mutex_lock((t_earsbufobj *)x);
    earsbufobj_init_progress((t_earsbufobj *)x, num_buffers);
    
    t_llllelem *el = x->envin->l_head;
   
    t_buffer_obj *mod = NULL;

    double modVal = 1;
    int modType = 0;

    for (long count = 0; count < num_buffers; count++) {
        t_buffer_obj *in = earsbufobj_get_inlet_buffer_obj((t_earsbufobj *)x, 0, count);
        
        t_buffer_obj *out = earsbufobj_get_outlet_buffer_obj((t_earsbufobj *)x, 0, count);
        
        
        
        t_llll *env = earsbufobj_llllelem_to_env_samples((t_earsbufobj *)x, el, in);
        
        if (hatom_gettype(&env->l_head->l_hatom) == H_SYM) {
            
            t_buffer_ref *ref = buffer_ref_new((t_object *)x, hatom_getsym(&env->l_head->l_hatom));
            mod = buffer_ref_getobject(ref);
            modType = 1;
            
        } else {
            modVal = hatom_getdouble(&el->l_hatom);
            modType = 2;
        }
        
        llll_free(env);
        
        repeatgliss_bang(x, in, out, mod, modVal, modType);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_repeatgliss_anything(t_buf_repeatgliss *x, t_symbol *msg, long ac, t_atom *av)
{
    long inlet = earsbufobj_proxy_getinlet((t_earsbufobj *) x);
    
    t_llll *parsed = earsbufobj_parse_gimme((t_earsbufobj *) x, LLLL_OBJ_VANILLA, msg, ac, av);
    if (!parsed) return;
    
    if (parsed && parsed->l_head) {
        if (inlet == 0) {
            long num_bufs = llll_get_num_symbols_root(parsed);
            earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_IN, 0, num_bufs, true);
            earsbufobj_resize_store((t_earsbufobj *)x, EARSBUFOBJ_OUT, 0, num_bufs, true);
            earsbufobj_store_buffer_list((t_earsbufobj *)x, parsed, 0);
            
            buf_repeatgliss_bang(x);
        
        } else if (inlet == 1) {
            llll_free(x->envin);
            x->envin = llll_clone(parsed);
        }
        
    }
    llll_free(parsed);
}


void repeatgliss_bang(t_buf_repeatgliss *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType) {
    
    t_float        *tab;
    t_float        *envelope;
 
    
    int minsampl = CLAMP(x->sampMin_in, 1, 5000);
    int ncross = CLAMP(x->cross_in, 1, 1000);
    int repeatMult = CLAMP(x->repeatMult_in, 1, 2000);
    float slopePitch = CLAMP(x->slopePitch_in, 0.001, 50);
    float slopeAmp = CLAMP(x->slopeAmp_in, 0.001, 50);
    
    int pitchEGtype = x->pitchEGtype_in;
    int ampEGtype = x->ampEGtype_in;
    
    int envPitch = x->envPitchOnOff_in;
    int envAmp = x->envAmpOnOff_in;
    
    int pitchMin = x->pitchMin_in;
    int pitchMax = x->pitchMax_in;

    
    
    long        frames, sampleRate, envelopeFrames ;
    
    tab = ears_buffer_locksamples(buffer);
    frames = buffer_getframecount(buffer);
    sampleRate = buffer_getsamplerate(buffer);
    
    envelope = ears_buffer_locksamples(mod);
    envelopeFrames = buffer_getframecount(mod);
    
    int nchan;
    nchan = buffer_getchannelcount(buffer);
    double *inbuffer =  (double*) sysmem_newptr(frames * sizeof(double));
    
    int maxcross = round(frames / (minsampl * ncross));
    int *zerocrossindex = (int*) sysmem_newptr(maxcross * sizeof(int));
    int maxmemory = (int)(frames);
    double *dataout =  (double*) sysmem_newptr(maxmemory * sizeof(double));
  
    int frameout = 0;
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int z, v , ver;
    for (z = 1 ; z < (nchan + 1) ; z++) {
        int inc = 0;
        
        for (v = 0 ; v < (frames * nchan) ; v++) {
            ver = (v % nchan) + 1;
            if (ver == z) {
                inc++;
                inbuffer[inc] = tab[v];
            }
        }
        
        int  g = 1, minsamplcount = 0, h = 0, j, k, ncrossindex = 0, n = 0,  currPeriod, newPeriod, u = 0, crosscount = 0, repeat;
        float risePitchEG,  fallPitchEG, riseAmpEG,  fallAmpEG, hanning;
        double bCF, aCF, resA, idxD, scaleCF;
        int a = 0, b = 0;
        
        // waveset segmentation
        for (j = 0 ; j < frames ; j++) {
            minsamplcount++;
            
            if (inbuffer [j] >= 0 && inbuffer [j - 1] <= 0 && minsamplcount >= minsampl) {
                minsamplcount = 0;
                ncrossindex++;
                
                if (ncrossindex == ncross) {
                    ncrossindex = 0;
                    crosscount++;
                    if (crosscount >= maxcross) {
                        maxcross = maxcross + round(maxcross/4);
                        zerocrossindex = (int*) sysmem_resizeptrclear(zerocrossindex, maxcross * sizeof(int));
                        
                    }
                    
                    
                    zerocrossindex[crosscount] = j;
          
                }
            }
        }
        
        
        
        //
        
        zerocrossindex[0] = 0;

        
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ricampiono il buffer di inviluppo e lo normalizzo al buffer dei waveset
        float *envOnset = (float *)bach_newptr(crosscount * sizeof(float));
        if (modType == 1) {
            ears_resample_linear(envelope, envelopeFrames, &envOnset, crosscount, ((float)crosscount/(float)envelopeFrames), 1);
        } else {
            for (int gg = 0 ; gg < crosscount ; gg++) {
                envOnset[gg] = modVal;
            
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        
        while (g <= crosscount) {
            
            currPeriod = (zerocrossindex[g] - zerocrossindex[g  - 1]);
            
            if (modType == 1) {
                repeat = (CLAMP(envOnset[g], 0 , 1) * repeatMult) + 1;
            } else {
                repeat = CLAMP(envOnset[g], 1, 5000);
            }
            
            while (u < repeat) {
                u++;
                
                if (repeat == 1) {
                    newPeriod = currPeriod;
                } else {
                    risePitchEG = pow((float)u/repeat, slopePitch);
                    fallPitchEG = 1. - pow(1. - (float)u/repeat, slopePitch);
                    riseAmpEG = pow((float)u/repeat, slopeAmp);
                    fallAmpEG = 1. - pow(1. - (float)u/repeat, slopeAmp);
                    
                    float pMin = ((float)currPeriod - 1) * pitchMin;
                    
                    
                    if (envPitch == 0) {
                        if (pitchEGtype == 0) {
                            newPeriod = CLAMP(pMin + ((currPeriod - pMin) * risePitchEG * pitchMax), 2, currPeriod * pitchMax);
                        } else {
                            newPeriod = CLAMP(pMin + ((currPeriod - pMin) * (1 - fallPitchEG) * pitchMax), 2, currPeriod * pitchMax);
                        }
                        
                    } else {
                        newPeriod = currPeriod;
                    }
                }
                
                
                float diff = fabs(inbuffer[zerocrossindex[g - 1]]) + fabs(inbuffer[zerocrossindex[g] - 1]);
        
                
                while (n < newPeriod) {

                 
                    scaleCF = ((double)currPeriod - 1) / ((double)newPeriod - 1);
                    idxD = (double)scaleCF * n;
                    a = (int)idxD;
                    b = a + 1;
                    bCF = idxD - a;
                    aCF = 1.0 - bCF;
                    resA = (double)(aCF * inbuffer[a + zerocrossindex[g - 1]] + bCF * inbuffer[b + zerocrossindex[g - 1]]);
  
                    
                    
                    if (h >= maxmemory) {
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                    }
                    
                    
                                   
                    float sampleCorrection = ((float)n/(newPeriod - 1)) * diff - inbuffer[zerocrossindex[g - 1]];
                     
                               
                    if (envAmp == 0) {
                        if (ampEGtype == 0) {
                            dataout[h] = (resA + sampleCorrection) * (1. - fallAmpEG);
                        } else {
                            dataout[h] = (resA + sampleCorrection) * (riseAmpEG);
                        }
                        
                    } else {
                        dataout[h] = resA + sampleCorrection;
                    }
             
                    
                    h++;
                    n++;
                }

                n = 0;
            }

            u = 0;
            g++;
        }
        
        
        if (z == 1) {
            frameout = h - 1;;
        }
        
        
        ears_buffer_set_size_and_numchannels((t_object *) x, out, frameout, nchan);
        ears_buffer_set_sr((t_object *) x, out, sampleRate);
        
        float *outtab = ears_buffer_locksamples(out);
        
        
        inc = 0;
        for (k = 0 ; k < (frameout * nchan) ; k++) {
            ver = (k % nchan) + 1;
            if (ver == z) {
                inc++;
                double f = dataout[inc];
                outtab[k] = f;
            }
            
        }
        ears_buffer_unlocksamples(out);
        bach_freeptr(envOnset);
    }
    
    ears_buffer_unlocksamples(buffer);
    sysmem_freeptr(dataout);
    sysmem_freeptr(zerocrossindex);
    sysmem_freeptr(inbuffer);

    
    return;
}
