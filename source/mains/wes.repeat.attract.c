/**
    @file
    wes.repeat.attract.c
 
    @name
    wes.repeat.attract~
 
    @realname
    wes.repeat.attract~
 
    @type
    object
 
    @module
    wes
 
    @author
    Marco Marasciuolo
 
    @digest
    Repeats waveset
 
    @description
    Repeat each waveset n time and gradually change the waveform period and amplitude to the next waveset.
 
    @discussion
 
    @category
    wes basic
 
    @keywords
    buffer, pitchrepeat, pitchrepeaterse, retrograde, retrogradation, time
 
    @seealso
    wes.repeat.overlap~ , wes.repeat.enveloping~ , wes.repeat.pendulum~, wes.repeat.simplify~,  wes.repeat.attract~
    
    @owner
    Marco Marasciuolo
 */

#include "ext.h"
#include "ext_obex.h"
#include "foundation/llllobj.h"
#include "foundation/llll_commons_ext.h"
#include "math/bach_math_utilities.h"
#include "ears.object.h"





typedef struct _buf_pitchrepeat {
    t_earsbufobj        e_ob;
    long sampMin_in;
    int  repeat_in;
    long nCross_in;
    t_llll  *envin;
    
} t_buf_pitchrepeat;




// Prototypes
t_buf_pitchrepeat*         buf_pitchrepeat_new(t_symbol *s, short argc, t_atom *argv);
void            buf_pitchrepeat_free(t_buf_pitchrepeat *x);
void            buf_pitchrepeat_bang(t_buf_pitchrepeat *x);
void            buf_pitchrepeat_anything(t_buf_pitchrepeat *x, t_symbol *msg, long ac, t_atom *av);

void buf_pitchrepeat_assist(t_buf_pitchrepeat *x, void *b, long m, long a, char *s);
void buf_pitchrepeat_inletinfo(t_buf_pitchrepeat *x, void *b, long a, char *t);

void wavesetrepeat_bang(t_buf_pitchrepeat *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType);

// Globals and Statics
static t_class    *s_tag_class = NULL;
static t_symbol    *ps_event = NULL;

EARSBUFOBJ_ADD_IO_METHODS(pitchrepeat)

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
    
    CLASS_NEW_CHECK_SIZE(c, "wes.repeat.attract~",
                         (method)buf_pitchrepeat_new,
                         (method)buf_pitchrepeat_free,
                         sizeof(t_buf_pitchrepeat),
                         (method)NULL,
                         A_GIMME,
                         0L);
    
    // @method symbol @digest Process buffers
    // @description A symbol with buffer name will trigger the buffer processing and output the processed
    // buffer name (depending on the <m>naming</m> attribute).
    EARSBUFOBJ_DECLARE_COMMON_METHODS_HANDLETHREAD(pitchrepeat)
    
    CLASS_ATTR_LONG(c, "minSamp", 0, t_buf_pitchrepeat, sampMin_in);
    CLASS_ATTR_LONG(c, "repeatMult", 0, t_buf_pitchrepeat, repeat_in);
    CLASS_ATTR_LONG(c, "nCross", 0, t_buf_pitchrepeat, nCross_in);

    earsbufobj_class_add_outname_attr(c);
    earsbufobj_class_add_blocking_attr(c);
    earsbufobj_class_add_naming_attr(c);
    
    earsbufobj_class_add_polyout_attr(c);

    class_register(CLASS_BOX, c);
    s_tag_class = c;
    ps_event = gensym("event");
}

void buf_pitchrepeat_assist(t_buf_pitchrepeat *x, void *b, long m, long a, char *s)
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

void buf_pitchrepeat_inletinfo(t_buf_pitchrepeat *x, void *b, long a, char *t)
{
    if (a)
        *t = 1;
}


t_buf_pitchrepeat *buf_pitchrepeat_new(t_symbol *s, short argc, t_atom *argv)
{
    t_buf_pitchrepeat *x;
    long true_ac = attr_args_offset(argc, argv);
    
    x = (t_buf_pitchrepeat*)object_alloc_debug(s_tag_class);
    if (x) {
        
        x->envin = llll_from_text_buf("1", false);
        x->sampMin_in = 15;
        x->repeat_in = 0;
        x->nCross_in = 1;
  
        earsbufobj_init((t_earsbufobj *)x,  0);
        
        // @arg 0 @name outnames @optional 1 @type symbol
        // @digest Output buffer names
        // @description @copy EARS_DOC_OUTNAME_ATTR
        
  
        
        t_llll *args = llll_parse(true_ac, argv);
        t_llll *names = earsbufobj_extract_names_from_args((t_earsbufobj *)x, args);
        
 
        
        attr_args_process(x, argc, argv);
        
        earsbufobj_setup((t_earsbufobj *)x, "E4", "E", names);
        
        llll_free(args);
        llll_free(names);
    }
    return x;
}


void buf_pitchrepeat_free(t_buf_pitchrepeat *x)
{
    earsbufobj_free((t_earsbufobj *)x);
}




void buf_pitchrepeat_bang(t_buf_pitchrepeat *x)
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
        
        wavesetrepeat_bang(x, in, out, mod, modVal, modType);
        
        if (earsbufobj_iter_progress((t_earsbufobj *)x, count, num_buffers)) break;
    }
    
    earsbufobj_mutex_unlock((t_earsbufobj *)x);

    earsbufobj_outlet_buffer((t_earsbufobj *)x, 0);
}


void buf_pitchrepeat_anything(t_buf_pitchrepeat *x, t_symbol *msg, long ac, t_atom *av)
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
            
            buf_pitchrepeat_bang(x);
        
        } else if (inlet == 1) {
            llll_free(x->envin);
            x->envin = llll_clone(parsed);
        }
        
    }
    llll_free(parsed);
}

void wavesetrepeat_bang(t_buf_pitchrepeat *x, t_buffer_obj *buffer, t_buffer_obj *out, t_buffer_obj *mod, double modVal, int modType) {
    
    t_float        *tab;
    t_float        *envelope;
    
    int minsampl = CLAMP(x->sampMin_in, 1, 10000);
    int repeatMult = CLAMP(x->repeat_in, 0, 5000);
    int ncross = CLAMP(x->nCross_in, 1, 1000);
    

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
    double *wavePosPeak = (double*) sysmem_newptr(maxcross * sizeof(double));
    double *waveNegPeak = (double*) sysmem_newptr(maxcross * sizeof(double));
    int maxmemory = (int)(frames * (MAX(repeatMult, modVal) + 1));
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
        
        int g = 1, minsamplcount = 0, h = 0, b = 0, j, k, ncrossindex = 0, a, n = 0,  currPeriod, newPeriod, nextPeriod, crosscount = 0, repeat ;
        double bCF, aCF, res, idxD, scaleCF, maxPosPeak = 0, maxNegPeak = 0, posVal = 0, negVal = 0;
        
        

        for (j = 0 ; j < frames ; j++) {
            minsamplcount++;

            
            if (inbuffer [j] > 0) {
                posVal = inbuffer [j];
            } else {
                negVal = inbuffer [j];
            }
            
            if (maxPosPeak < posVal) {
                maxPosPeak = posVal;
            }
            
            
            if (maxNegPeak > negVal) {
                maxNegPeak = negVal;
            }

            if (inbuffer [j] >= 0 && inbuffer [j - 1] <= 0 && minsamplcount > minsampl) {
                minsamplcount = 0;
                ncrossindex++;
                
                if (ncrossindex == ncross) {
                    ncrossindex = 0;
                    crosscount++;
                    if (crosscount >= maxcross) {
                        maxcross = maxcross + round(maxcross/4);
                        zerocrossindex = (int*) sysmem_resizeptrclear(zerocrossindex, maxcross * sizeof(int));
                        wavePosPeak = (double*) sysmem_resizeptrclear(wavePosPeak, maxcross * sizeof(double));
                        waveNegPeak = (double*) sysmem_resizeptrclear(waveNegPeak, maxcross * sizeof(double));
                    }
                    
                    zerocrossindex[crosscount] = j;
                    wavePosPeak[crosscount] = maxPosPeak;
                    waveNegPeak[crosscount] = maxNegPeak;
                    
                    maxPosPeak = 0;
                    maxNegPeak = 0;
                }
                
            }
        }
        
        wavePosPeak[0] = 0;
        waveNegPeak[0] = 0;
        zerocrossindex[0] = 0;
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        float *envOnset = (float *)bach_newptr(crosscount * sizeof(float));
        if (modType == 1) {
            
             ears_resample_linear(envelope, envelopeFrames, &envOnset, crosscount, ((float)crosscount/(float)envelopeFrames), 1);
        } else {
           
            for (int gg = 0 ; gg < crosscount ; gg++) {
                envOnset[gg] = modVal;
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        while (g <= crosscount && (g + 1) <= crosscount) {
            
            if (modType == 1) {
                repeat = round(CLAMP(envOnset[g], 0 , 1) * repeatMult);
            } else {
                repeat = CLAMP(modVal, 0 , 5000);
            }
            
            int r = 0;
            currPeriod = (zerocrossindex[g] - zerocrossindex[g  - 1]);
            if (repeat == 0) {
                for (int e = 0 ; e < currPeriod ; e++) {
                    if (h >= maxmemory) {
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                    }
                    dataout[h] = inbuffer[e + zerocrossindex[g  - 1]];
                    h++;
                }
            } else {
            
            while (r < repeat) {
                r++;
                
                currPeriod = (zerocrossindex[g] - zerocrossindex[g  - 1]);
                
                if (g == crosscount) {
                    nextPeriod = currPeriod;
                } else {
                    nextPeriod = (zerocrossindex[g + 1] - zerocrossindex[g]);
                }
                
                
            
                
                double med = pow((float)r/repeat, 2) * ((double)nextPeriod - (double)currPeriod);
                
                
                newPeriod = round(currPeriod + med);
                
                
                double newPosGainFactor = (wavePosPeak[g] + (r * (wavePosPeak[g+1] - wavePosPeak[g])/ repeat))/ wavePosPeak[g] ;
                
                double newNegGainFactor = (waveNegPeak[g] + (r * (waveNegPeak[g+1] - waveNegPeak[g])/ repeat))/ waveNegPeak[g] ;
                
           
                
                if (wavePosPeak[g] == 0 || wavePosPeak[g+1] == 0) {
                    newPosGainFactor = 0;
                }
                
                if (waveNegPeak[g] == 0 || waveNegPeak[g+1] == 0) {
                    newNegGainFactor = 0;
                }
                
                
                while (n < newPeriod) {
        
                    
                    scaleCF = ((double)currPeriod -1) / ((double)newPeriod -1);
                    idxD = (double)scaleCF * n;
                    a = (int)idxD;
                    b = a + 1;
                    bCF = idxD - a;
                    aCF = 1.0 - bCF;
                    res = (double)(aCF * inbuffer[a + zerocrossindex[g - 1]] + bCF * inbuffer[b + zerocrossindex[g - 1]]);
                    
                    
                    
                    if (h >= maxmemory) {
                        
                        maxmemory = maxmemory + round(maxmemory/4);
                        dataout =  (double*)sysmem_resizeptrclear(dataout, maxmemory * sizeof(double));
                    }
                    
                    if (res >= 0) {
                        dataout[h] = res * newPosGainFactor;
                    } else {
                        dataout[h] = res * newNegGainFactor;
                    }
                    
           
                    n++;
                    h++;
                }
                n = 0;
            }
            }
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
    
    
    buffer_unlocksamples(buffer);
    sysmem_freeptr(zerocrossindex);
    sysmem_freeptr(wavePosPeak);
    sysmem_freeptr(waveNegPeak);
    sysmem_freeptr(dataout);
    sysmem_freeptr(inbuffer);
    return;
}
