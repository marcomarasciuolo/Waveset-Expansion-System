
/**
@file
wes.waveform.reduction.c

@name
wes.waveform.reduction~

@realname
wes.waveform.reduction~

@type
object

@module
waveset

@author
Marco Marasciuolo

@digest
 Repeats waveform n. times

@description
select a waveset every n. and apply the waveform to subsequent wavesets.

@discussion

@category
waveset basic

@keywords
buffer, waveset

@seealso
 wes.waveform.uniform~, wes.waveform.lag~,  wes.waveform.shift~,  wes.waveform.interpolate~

@owner
Marco Marasciuolo
 */

#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"





typedef struct _buf_wavereduction {
    t_earsbufobj        e_ob;
    long sampMin_in;
    long repeat_in;
    long nCross_in;
    int interp;
    t_llll  *envin;

    
} t_buf_wavereduction;




// Prototypes
t_buf_wavereduction*         buf_wavereduction_new(t_symbol *s, short argc, t_atom *argv);
void            buf_wavereduction_free(t_buf_wavereduction *x);
void            buf_wavereduction_bang(t_buf_wavereduction *x);
void            buf_wavereduction_anything(t_buf_wavereduction *x, t_symbol *msg, long ac, t_atom *av);

void buf_wavereduction_assist(t_buf_wavereduction *x, void *b, long m, long a, char *s);
void buf_wavereduction_inletinfo(t_buf_wavereduction *x, void *b, long a, char *t);

void wavereduction_bang(t_buf_wavereduction *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType);

// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(wavereduction)

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
    
    CLASS_NEW_CHECK_SIZE(c, "wes.waveform.reduction~",
                         (method)buf_wavereduction_new,
                         (method)buf_wavereduction_free,
                         sizeof(t_buf_wavereduction),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
    // @method symbol @digest Process buffers
    // @description A symbol with buffer name will trigger the buffer processing and output the processed
    // buffer name (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(wavereduction)
    
    CLASS_ATTR_LONG(c, "minSamp", 0, t_buf_wavereduction, sampMin_in);
    CLASS_ATTR_LONG(c, "repeatMult", 0, t_buf_wavereduction, repeat_in);
    CLASS_ATTR_LONG(c, "nCross", 0, t_buf_wavereduction, nCross_in);
    //CLASS_ATTR_LONG(c, "interp", 0, t_buf_wavereduction, interp);
    
    CLASS_ATTR_CHAR(c, "InterpActivate", 0, t_buf_wavereduction, interp);
        CLASS_ATTR_STYLE_LABEL(c,"InterpActivate",0,"enumindex","Interpolation Activate");
        CLASS_ATTR_ENUMINDEX(c,"InterpActivate", 0, "Off On");


    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_wavereduction_assist(t_buf_wavereduction *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        if (a == 0) // @in 0 @type symbol @digest Buffer name(s)
            sprintf(s, "symbol: Buffer Names");
        else if (a == 1) // @in 1 @type number/symbol @digest number of waveform repeat
            sprintf(s, "Buffer/float: number of waveform repeat"); // @number of waveform repeat
    } else {
        sprintf(s, "New Buffer Names"); // @out 0 @type symbol/list @digest Output buffer names(s)
                                            // @description Name of the buffer
    }
}

void buf_wavereduction_inletinfo(t_buf_wavereduction *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_wavereduction *buf_wavereduction_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_wavereduction *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_wavereduction*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->envin = llll_from_text_buf("1", false);
        x->sampMin_in = 100;
        x->repeat_in = 1;
        x->nCross_in = 1;
        x->interp = 1;

  
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


void buf_wavereduction_free(t_buf_wavereduction *x)
{
    llll_free(x->envin);
    earsbufobj_free((t_earsbufobj *)x);
}




void buf_wavereduction_bang(t_buf_wavereduction *x)
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
        
        wavereduction_bang(x, in, out, mod, modVal, modType);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_wavereduction_anything(t_buf_wavereduction *x, t_symbol *msg, long ac, t_atom *av)
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
            
            buf_wavereduction_bang(x);
        
        } else if (inlet == 1) {
            llll_free(x->envin);
            x->envin = llll_clone(parsed);
        }
        
    }
    llll_free(parsed);
}


void wavereduction_bang(t_buf_wavereduction *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType) {
    
    t_float        *tab;
    t_float        *envelope;
    
    int minsampl = CLAMP(x->sampMin_in, 1, 5000);
    int ncross = CLAMP(x->nCross_in, 1, 1000);
    int repeatMult = CLAMP(x->repeat_in, 1, 5000);
    int interpwave = CLAMP(x->interp, 0, 1);
    
 
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
    double *peakVal = (double*) sysmem_newptr(maxcross * sizeof(double));
    
    int maxmemory = (int)(frames);
    double *dataout =  (double*) sysmem_newptr(maxmemory * sizeof(double));
    
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int z, v , ver, frameout = 0;
    for (z = 1 ; z < (nchan + 1) ; z++) {
        int inc = 0;
        
        for (v = 0 ; v < (frames * nchan) ; v++) {
            ver = (v % nchan) + 1;
            if (ver == z) {
                inc++;
                inbuffer[inc] = tab[v];
            }
        }
        int  g = 1, minsamplcount = 0, h = 0, j, k, a = 0,b, ncrossindex = 0, n = 0,d = 0, currPeriod, newPeriod,interpNextPeriod, repeat = 1, crosscount = 0, window = 0, nextPeriod;
        
        double maxPeak = 0, newPeakVal, nextPeakVal, currPeakVal, peakFactorA, peakFactorB;
        double bCF, aCF, resA, resB, idxD, scaleCF;
        
        // waveset segmentation
        for (j = 0 ; j < frames ; j++) {
            minsamplcount++;
            
            if (maxPeak < fabs(inbuffer[j])) {
                maxPeak = fabs(inbuffer[j]);
            }
            
            
            if (inbuffer [j] >= 0 && inbuffer [j - 1] <= 0 && minsamplcount >= minsampl) {
                minsamplcount = 0;
                ncrossindex++;
                
                if (ncrossindex == ncross) {
                    ncrossindex = 0;
                    crosscount++;
                    if (crosscount >= maxcross) {
                        maxcross = maxcross + round(maxcross/4);
                        zerocrossindex = (int*) sysmem_resizeptrclear(zerocrossindex, maxcross * sizeof(int));
                        peakVal = (double*) sysmem_resizeptrclear(peakVal, maxcross * sizeof(double));
                        
                    }
                    
                    
                    zerocrossindex[crosscount] = j;
                    peakVal[crosscount] = maxPeak;
                    maxPeak = 0;
                    
                    
                }
            }
        }
        
        
        //
        
        zerocrossindex[0] = 0;
        peakVal[0] = 0;
        
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
        float *envOnset = (float *)bach_newptr(crosscount * sizeof(float));
        if (modType == 1) {
            ears_resample_linear(envelope, envelopeFrames, &envOnset, crosscount, ((float)crosscount/(float)envelopeFrames), 1);
        } else {
            for (int gg = 0 ; gg < crosscount ; gg++) {
                envOnset[gg] = CLAMP(modVal, 0, 5000);
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        
        while (g <= crosscount) {
            d = 0;
            
            if (modType == 1) {
                repeat = round(CLAMP(envOnset[g - 1],0 ,1 ) * repeatMult) + 0;
            } else {
                repeat = envOnset[g - 1];
            }
            
            if (repeat == 0) {
                for (int l = 0 ; l < (zerocrossindex[g] - zerocrossindex[g  - 1]) ; l++) {
                    if (h >= maxmemory) {
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout = (double*) sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                    }
                    dataout[h] = inbuffer[l + zerocrossindex[g - 1]];
                    h++;
                }
                g++;
            } else {
                
                while (d < repeat && (d + g) <= crosscount) {
                    
                    currPeriod = (zerocrossindex[g] - zerocrossindex[g  - 1]);
                    newPeriod = (zerocrossindex[g + d] - zerocrossindex[(g + d) - 1]);
                    nextPeriod = (zerocrossindex[g + repeat] - zerocrossindex[(g + repeat) - 1]);
                    currPeakVal = peakVal[g];
                    nextPeakVal = peakVal[g + repeat];
                    newPeakVal = peakVal[g + d];
                    
                    if ( (g + repeat) > crosscount) {
                        interpNextPeriod = currPeriod;
                    } else {
                        interpNextPeriod = (zerocrossindex[g + repeat] - zerocrossindex[(g + repeat)  - 1]);
                    }
                    
                    int segmentDur = zerocrossindex[(g + repeat) - 1] - zerocrossindex[g  - 1];
                    
                    
                    for (int n = 0 ; n < newPeriod ; n++) {
                        
                        scaleCF = ((double)currPeriod -1 ) / ((double)newPeriod -1);
                        idxD = (double)scaleCF * n;
                        a = (int)idxD;
                        b = a + 1;
                        bCF = idxD - a;
                        aCF = 1.0 - bCF;
                        resA = (double)(aCF * inbuffer[a + zerocrossindex[g - 1]] + bCF * inbuffer[b + zerocrossindex[g - 1]]);
                        
                        if (interpwave == 1 && (g + repeat) < crosscount) {
                            scaleCF = ((double)nextPeriod -1 ) / ((double)newPeriod -1);
                            idxD = (double)scaleCF * n;
                            a = (int)idxD;
                            b = a + 1;
                            bCF = idxD - a;
                            aCF = 1.0 - bCF;
                            resB = (double)(aCF * inbuffer[a + zerocrossindex[(g + repeat) - 1]] + bCF * inbuffer[b + zerocrossindex[(g + repeat) - 1]]);
                        }
                        
                        if (currPeakVal == 0) {
                            peakFactorA = 0;
                        } else {
                            peakFactorA = newPeakVal/currPeakVal;
                        }
                        
                        if (nextPeakVal == 0) {
                            peakFactorB = 0;
                        } else {
                            peakFactorB = newPeakVal/nextPeakVal;
                        }
                        
                        
                        float fadeIn = sin(((float)window/(float)segmentDur) * (PI * 0.5));
                        float fadeOut = cos(((float)window/(float)segmentDur) * (PI * 0.5));
                        
                        if (h >= maxmemory) {
                            maxmemory = maxmemory + round(maxmemory/4);
                            dataout = (double*) sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                        }
                        
                        
                        if (interpwave == 1 && (g + repeat) < crosscount) {
                            
                            dataout[h] = ((resA * peakFactorA) * fadeOut) + ((resB * peakFactorB) * fadeIn) ;
                            
                            
                        } else {
                            
                            dataout[h] = resA * peakFactorA;
                        }
                        
                        h++;
                        window++;
                    }
                    
                    d++;
                    n = 0;
                }
            }
            window = 0;
            g = g + d;
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
    buffer_unlocksamples(mod);
    sysmem_freeptr(dataout);
    sysmem_freeptr(zerocrossindex);
    sysmem_freeptr(peakVal);
    sysmem_freeptr(inbuffer);
    
    
    return;
}
