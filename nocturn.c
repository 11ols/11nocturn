/**
	11nocturn - Max external using libusb to connect with a Novation Nocturn 

	Device vendor and product id:
	VID 0x1235
	PID 0x000a

	CONFIG 1        
	INTF 0

	Device endpoints:
	EP_IN 0x81
	EP_OUT 0x02
*/

#include "ext.h"							
#include "ext_obex.h"						
#include "libusb.h"


// object struct
typedef struct _nocturn
{
	t_object	ob;
	t_atom		val;
	t_symbol	*name;

    void *retry_clock;
    void *outlet_clock;
    
    void *out; //outlets
    void *outb;
    void *outc;
    void *outd;
    void *oute;
    void *outf;
    int sleeptime;
    t_systhread         writethread;
    t_systhread         readthread;
    t_systhread_mutex   read_mutex;
    t_systhread_mutex   write_mutex;
    int                readthread_cancel;      // thread exit flag
    int                writethread_cancel;     // thread exit flag2
    int                 x_start;                // offset for cycle buffer
    int                x_send;                  // buffer length
    unsigned char wrtmp[128];                   // buffer to send
    unsigned char dataIn[8][8];                 // received data
    int                maxw;                    // verbose output to maxwindow?
    
    libusb_device_handle *dev_handle;           //a device handle
    libusb_context *ctx;                        //a libusb session
    
    int retry_warning_pushed;
    
} t_nocturn;

///////////////////////// function prototypes
void *nocturn_new(t_symbol *s, long argc, t_atom *argv);
void nocturn_free(t_nocturn *x);
void nocturn_assist(t_nocturn *x, void *b, long m, long a, char *s);

//void nocturn_int(t_nocturn *x, long n);
//void nocturn_float(t_nocturn *x, double f);
//void nocturn_anything(t_nocturn *x, t_symbol *s, long ac, t_atom *av);
//void nocturn_bang(t_nocturn *x);

void nocturn_identify(t_nocturn *x);
void nocturn_dblclick(t_nocturn *x);
void nocturn_acant(t_nocturn *x);

void nocturn_start_low(t_nocturn *x);
void nocturn_start(t_nocturn *x);
void nocturn_maxwindow(t_nocturn *x, long n);
void nocturn_state(t_nocturn *x, long n);
void nocturn_send2(t_nocturn *x, t_symbol *s, long argc, t_atom *argv);
void nocturn_send(t_nocturn *x, t_symbol *s, long argc, t_atom *argv);
void nocturn_stop(t_nocturn *x);
void *nocturn_read(t_nocturn *x);
void *nocturn_write(t_nocturn *x);
void nocturn_feed_outlets(t_nocturn *x);
void nocturn_test(t_nocturn *x, t_symbol *s, long argc, t_atom *argv);
void nocturn_sleeptime(t_nocturn *x, long n);
void nocturn_connection_error(t_nocturn *x);
void nocturn_connection_success(t_nocturn *x);
void nocturn_restart(t_nocturn *x);

void *nocturn_class; // global class pointer variable




void ext_main(void *r)
{
	t_class *c;

	c = class_new("11nocturn", (method)nocturn_new, (method)nocturn_free, (long)sizeof(t_nocturn),
				  0L /* leave NULL!! */, A_GIMME, 0);

    class_addmethod(c, (method)nocturn_assist,           "assist",        A_CANT, 0);  /* you CAN'T call this from the patcher */
    class_addmethod(c, (method)nocturn_send,             "send",           A_GIMME, 0);
    class_addmethod(c, (method)nocturn_maxwindow,        "maxwindow",    A_LONG, 0);
    class_addmethod(c, (method)nocturn_state,           "state",    A_LONG, 0);
    class_addmethod(c, (method)nocturn_test,             "test",           A_GIMME, 0);
    class_addmethod(c, (method)nocturn_sleeptime,        "sleeptime",    A_LONG, 0);
    
    class_register(CLASS_BOX, c); /* CLASS_NOBOX */
    nocturn_class = c;
}


void nocturn_start_low(t_nocturn *x)                                              
{
	defer_low(x, (method)nocturn_start, NULL, 0, NULL);
}

void nocturn_start(t_nocturn *x)                                              /// START
{
    int r; //for return values
	int ret; int actual;
	unsigned char tmp[3] = { 0, 0, 176 }; // seems that it needs to be exactly these 3 bytes

    x->ctx = NULL; //a libusb session
    
    r = libusb_init(&x->ctx); //initialize the library for the session
    if(r < 0)
    {
        object_error((t_object *)x,"libusb_init error: %s", libusb_strerror(r)); //there was an error with libusb
        return;
    }
    
    libusb_set_debug(x->ctx, 3); //set verbosity level to 3, as suggested in the documentation
    
    
    // --> we don't need the device list if we use libusb_open_device_with_vid_pid() //
    
//    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
//    ssize_t cnt; //holding number of devices in list
//
//    cnt = libusb_get_device_list(x->ctx, &devs); //get the list of devices
//    if(cnt < 0)
//    {
//        object_error((t_object *)x,"error getting device list");
//        return;
//    }
//    object_post((t_object *)x,"%d devices in list.", cnt);
    
    
    
    x->dev_handle = libusb_open_device_with_vid_pid(x->ctx, 0x1235, 0x000a); //these are vendorID and productID
    
    if(x->dev_handle == NULL)
    {
        if (!x->retry_warning_pushed)
            object_error((t_object *)x,"Cannot open Nocturn device!");
        //libusb_free_device_list(devs, 1); //free the list, unref the devices in it
        goto RETRY;
    }
    else
    {
        object_post((t_object *)x,"Nocturn device opened");
        //libusb_free_device_list(devs, 1); //free the list, unref the devices in it
    }

    
    r = libusb_set_configuration(x->dev_handle, 1);
    if (r < 0)
	{	
		if (!x->retry_warning_pushed) 
			object_error((t_object *)x, "error setting config %d: %s", 1, libusb_strerror(r));
	}
    
    r = libusb_claim_interface(x->dev_handle, 0); //claim interface 0 (the first)
    if (r < 0 )
    {
		if (!x->retry_warning_pushed)
			object_error((t_object *)x, "Cannot claim interface: %s", libusb_strerror(r));
        libusb_close(x->dev_handle); //close the device we opened
        goto RETRY;
    }
      
	// all good now trying to send init

	ret = libusb_interrupt_transfer(x->dev_handle, 0x02, tmp, 3, &actual, 0);
	if (ret != 0)
	{
		object_error((t_object *)x, "error sending init to nocturn: %s", libusb_strerror(ret));
	}

	// start threads

    object_post((t_object *)x, "starting read-thread");
    systhread_create((method) nocturn_read, x, 0, 0, 0, &x->readthread);
        
    object_post((t_object *)x, "starting write-thread");
    systhread_create((method) nocturn_write, x, 0, 0, 0, &x->writethread);
    
	defer_low(x, (method)nocturn_connection_success, NULL, 0, NULL);
    return; // success
    
    
	RETRY:

	libusb_exit(x->ctx); x->ctx = NULL;

    if (!x->retry_warning_pushed)
    {
		defer_low(x, (method)nocturn_connection_error, NULL, 0, NULL);
        x->retry_warning_pushed = true;
    }
	
	clock_delay(x->retry_clock, 10000); // start again in 10 sec

}


void nocturn_connection_error(t_nocturn *x)
{
	object_error((t_object *)x, "Error initializing Novation Nocturn USB device!");
	object_error((t_object *)x, "Nocturn connected? Device blocked?");
	#if defined(_WIN64)
	object_error((t_object *)x, "Did you install the WinUSB driver for the Nocturn?");
	#endif
	object_warn((t_object *)x, "The object keeps searching for the device...");
	outlet_int(x->outf, 0);
}
void nocturn_connection_success(t_nocturn *x)
{
	outlet_int(x->outf, 1);
}
void nocturn_restart(t_nocturn *x)
{
	nocturn_stop(x);
	nocturn_start(x);
}

void nocturn_send(t_nocturn *x, t_symbol *s, long argc, t_atom *argv)            /// SEND
{
    int i;
    int overflow=false;
    int send; int start;
    int buffersize = sizeof(x->wrtmp);
    
    if (argc!=2)
    {
        if (x->maxw)
            object_warn((t_object *)x, "for correct buffering use send + list of 2 numbers only");
    }
    if (x->writethread_cancel || !x->writethread)
    {
        return ;
    }
    
    systhread_mutex_lock(x->write_mutex);
    if (argc!=2) x->x_send = 0; // reset buffer for unusual message
    
    send = x->x_send;
    start = x->x_start;
    for (i = send; i < (send+argc); i++ )   //append to buffer
    {
        if (i >= buffersize)
        {
            //buffer overflow, starts to overwrite oldest
            overflow = true;
            x->wrtmp[(i+start)%buffersize] = atom_getlong(argv+(i-send));
            x->x_start = (x->x_start + 1)%buffersize;
        }else
        {  
            x->wrtmp[(i+x->x_start)%buffersize] = atom_getlong(argv+(i-send));
            x->x_send++;
        }
    }
    systhread_mutex_unlock(x->write_mutex);
    
	if (overflow && x->maxw && x->writethread) 
    {
        //outlet_bang(x->outf);
        object_error((t_object *)x, "BUFFER OVERFLOW, \nmessage rate to object inlet is to high");
    }
    //else object_post((t_object *)x, "buffer size: %d", i);
}



void *nocturn_write(t_nocturn *x)                                       /// WRITING-THREAD
{
    unsigned char tmp[8];
    int send;
    int actual; //used to find out how many bytes were written
    int ret;
    int i;
    int buffersize = sizeof(x->wrtmp);
    
    while(1) // loop of the thread, exit with "break"
    {
        
        //if (x->writethread_cancel) break; // test if we're being asked to die
        
        systhread_mutex_lock(x->write_mutex);
        if (x->writethread_cancel)
        {
            systhread_mutex_unlock(x->write_mutex);
            break;
        }
        if (x->x_send) // do we have something to send?
        {
            // copy values and decrease x_send
            send = x->x_send;
            for (i=0;i<send && i<8;i++) //extract the first 8 or less values from buffer
            {
                tmp[i] = x->wrtmp[(i + x->x_start)%buffersize];
                x->x_send--;
            }
            if (x->x_send > 0 )// more values, set offset
            {
                x->x_start = (x->x_start + 8)%buffersize;
            }
            systhread_mutex_unlock(x->write_mutex);
            
            if(x->maxw)
            {
                if (i==0) object_post((t_object *)x, "nothing to send" );
                if (i==1) object_post((t_object *)x, "send:  %d \n", tmp[0]);
                if (i==2)object_post((t_object *)x, "send:  %d %d \n", tmp[0], tmp[1] );
                if (i==3) object_post((t_object *)x, "send:  %d %d %d \n", tmp[0], tmp[1], tmp[2] );
                if (i==4)object_post((t_object *)x, "send:  %d %d %d %d \n", tmp[0], tmp[1], tmp[2], tmp[3] );
                if (i==5)object_post((t_object *)x, "send:  %d %d %d %d %d \n", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4] );
                if (i==6)object_post((t_object *)x, "send:  %d %d %d %d %d %d \n", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5] );
                if (i==7)object_post((t_object *)x, "send:  %d %d %d %d %d %d %d \n", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6] );
                if (i==8)object_post((t_object *)x, "send:  %d %d %d %d %d %d %d %d\n", tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7] );
            }
            
            ret = 0; actual = 0;
            
            
            ret = libusb_interrupt_transfer(x->dev_handle, 0x02, tmp, i, &actual, 0);
            
            
            if (ret != 0)
            {
                object_error((t_object *)x, "error writing to nocturn: %s", libusb_strerror(ret) );
				defer_low(x, (method)nocturn_restart, NULL, 0, NULL);
				break;
            }
            else
            {
                if(x->maxw) object_post((t_object *)x, "success: written %d bytes, handled:%d ", actual, i);
            }

        }
        else //x_send is not set, no job
        {
            systhread_mutex_unlock(x->write_mutex);
        }
        
        systhread_sleep(x->sleeptime);

    } //end of "while loop"
    
    // this object_post causes freeze when object is freed (closing Max)
    //object_post((t_object *)x, "stopped writing-thread");
    // better no Max Api calls at the end of thread
    
    systhread_exit(0);
    return NULL;
}



void *nocturn_read(t_nocturn *x)                                        /// READING-THREAD
{
    unsigned char tmp[8];
    int ret;
    int error;
    int i;
    int oi;
    
    while(1)
    {
        memset(tmp, 0, sizeof(tmp));
        
        error = libusb_interrupt_transfer(x->dev_handle, 0x81, tmp, sizeof(tmp), &ret, 0);
        
        if (x->readthread_cancel) // test if we're being asked to die
            break;

		if ((int)tmp[0] + (int)tmp[1] + (int)tmp[2] + (int)tmp[3] + (int)tmp[4] +
			(int)tmp[5] + (int)tmp[6] + (int)tmp[7] == 0) //test if nocturn has been disconnected
		{
			defer_low(x, (method)nocturn_restart, NULL, 0, NULL);
			break;
		}
        
        if (error != 0)
        {
            object_error((t_object *)x, "reading-error %d : %s",error, libusb_strerror(error) );
			defer_low(x, (method)nocturn_restart, NULL, 0, NULL);
			break;
        }
        
        if(x->maxw) // maxwindow output
        {
            object_post((t_object *)x, "raw INPUT: %d %d %d %d %d %d %d %d",
                    tmp[0],tmp[1], tmp[2],tmp[3], tmp[4],tmp[5], tmp[6],tmp[7]);
        }
        
        // append the data to input buffer
        
        systhread_mutex_lock(x->read_mutex);
        
        for (i=0;i<8;i++)
        {
            if (x->dataIn[i][0] == 0)// find free slot
            {
               //put data in and break
                for (oi=0;oi<8;oi++)
                {
                    x->dataIn[i][oi] = tmp[oi];
                }
                break;
            }
            
        }
        systhread_mutex_unlock(x->read_mutex);
        
        clock_delay(x->outlet_clock, 0); //trigger
        
        
    } //end of "while loop"
    
    
    systhread_exit(0);                         // this can return a value to systhread_join();
    return NULL;
}



void nocturn_feed_outlets(t_nocturn *x)                             /// FEED_OUTLETS
{
    unsigned char tmp[8];
    int i; int oi;
    t_atom        a[2];
    int jobdone; int needrestart;
    jobdone = false;
    needrestart = false;
    
    systhread_mutex_lock(x->read_mutex);
    
    if (x->dataIn[0][0] == 0)// there is no job
    {
        systhread_mutex_unlock(x->read_mutex);
        return;
    }
    
    // we have at least 1 job, get first job data
    for (i=0;i<8;i++)
    {
        tmp[i] = x->dataIn[0][i];
    }
     
    for (i=1;i<8;i++) //reorder
    {
        for (oi=0;oi<8;oi++)
        {
            x->dataIn[i-1][oi] = x->dataIn[i][oi]; // move line 1 down
        }
        if (x->dataIn[i][0] == 0)
        {
            if (i!=1) needrestart = true;
            break;
        }
         
        if (i == 7) // fill last line with zeros if we ever get here
        {
            for (oi=0;oi<8;oi++)
            {
                x->dataIn[7][oi] = 0;
            }
        }
        
    }
    systhread_mutex_unlock(x->read_mutex);
    
    
    
   
    for (i=0;i<8;i++) // parse and route the data
    {
        if (tmp[i] == 176) //simply skip
        {
            continue;
        }
        else if (tmp[i] == 0) // it's the end
        {
          break;
        }
        else
        {
            // this and the next number are a pair
            if (x->maxw) object_post((t_object *)x, "%d %d", tmp[i], tmp[i+1] );
            
            
            
            if (tmp[i]>=112 && tmp[i]<=127)     // buttons
            {
                atom_setlong(a+0, tmp[i] - 112);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->out, 0, 2, a);

            } else if (tmp[i]==81)              // speed dial button
            {
                atom_setlong(a+0, 16);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->out, 0, 2, a);

            } else if (tmp[i]==72)              // fader
            {
                if (x->maxw) object_warn((t_object *)x, "crossfader input");
                outlet_int(x->outb, tmp[i+1]);

            } else if (tmp[i]>=64 && tmp[i]<=71) //encoder
            {
                atom_setlong(a+0, tmp[i]-64);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->outc, 0, 2, a);

            } else if (tmp[i]==74)               //speed dial
            {
                atom_setlong(a+0, 8);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->outc, 0, 2, a);

            } else if (tmp[i]>=96 && tmp[i]<=103)  //encoders touch
            {
                atom_setlong(a+0, tmp[i]-96);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->outd, 0, 2, a);

            } else if (tmp[i]==82)                  //speed dial touch
            {
                atom_setlong(a+0, 8);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->outd, 0, 2, a);
                
            } else if (tmp[i]==83)                  //fader touch
            {
                atom_setlong(a+0, 9);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->outd, 0, 2, a);
            }

                else                                //rest to outlet 5
            {
                atom_setlong(a+0, tmp[i]);
                atom_setlong(a+1, tmp[i+1]);
                outlet_list(x->oute, 0, 2, a);
            }
            
            i++; //skip next
        }
        
    } // end of for-loop
    
    if (needrestart) clock_delay(x->outlet_clock, 0);
    
}



void nocturn_stop(t_nocturn *x)                                                  // STOP //
{
	int ret; int actual;
	unsigned char tmp[2] = {0,0};
   

    clock_unset(x->retry_clock); // in case we are in the start-loop
    clock_unset(x->outlet_clock);
    

    if (x->writethread)
    {
        systhread_mutex_lock(x->write_mutex);
        x->writethread_cancel = true;
        systhread_mutex_unlock(x->write_mutex);
        systhread_join(x->writethread, &ret);// wait for the thread to stop
        x->writethread = NULL;
		x->x_send = 0;	// reset output buffer for restart
        x->writethread_cancel = false;
        object_post((t_object *)x, "stopped writing-thread");
		// trying to switch off all led
		ret = libusb_interrupt_transfer(x->dev_handle, 0x02, tmp, 2, &actual, 0);
    }
    if (x->readthread)
    {
        x->readthread_cancel = true;
        libusb_reset_device(x->dev_handle);
        systhread_join(x->readthread, &ret);// wait for the thread to stop
        x->readthread = NULL;
        x->readthread_cancel = false;
        object_post((t_object *)x, "stopped reading-thread");
    }

    if (x->dev_handle)
    {
        libusb_release_interface(x->dev_handle, 0);
        libusb_close(x->dev_handle);
        x->dev_handle = NULL;

    }
    if (x->ctx)
    {
        libusb_exit(x->ctx);
        x->ctx=NULL;
    }
    object_post((t_object *)x, "Nocturn released.");
    
}



void nocturn_free(t_nocturn *x)                                                 // FREE //
{
    nocturn_stop(x); // stop threads and usb device
                                          
    if (x->read_mutex)   // free out mutex
    systhread_mutex_free(x->read_mutex);
    if (x->write_mutex)   // free out write_mutex
    systhread_mutex_free(x->write_mutex);
    
    freeobject((t_object *)x->retry_clock);
    freeobject((t_object *)x->outlet_clock);
    
    //if (x->x_qelem)    // free our qelem
    //qelem_free(x->x_qelem);
}



void nocturn_state(t_nocturn *x, long n)
{
    if (n==0)
    {
        //object_post((t_object *)x, "stop");
        nocturn_stop(x); // stop threads and release usb device
    }
    else if (n==1)
    {
        if (x->readthread && x->writethread )
        {
        object_post((t_object *)x, "already running !");
        }
        
        else
        {
            object_post((t_object *)x, "restarting");
			x->retry_warning_pushed = 0;
            nocturn_start(x);
        }
    }
}
void nocturn_sleeptime(t_nocturn *x, long n)
{
    x->sleeptime=n;
}
void nocturn_maxwindow(t_nocturn *x, long n)
{
    if (n==0 || n==1) x->maxw=n;
}
void nocturn_test(t_nocturn *x, t_symbol *s, long argc, t_atom *argv)
{
	;
}


void *nocturn_new(t_symbol *s, long argc, t_atom *argv)                          /// NEW
{
    t_nocturn *x = NULL; int i; int j;
        
    x = (t_nocturn *)object_alloc(nocturn_class);
    object_post((t_object *)x, "11OLSEN.DE 2020/08/24");
    
    // if ( argc > 0 && (argv + 0)->a_type == A_LONG && atom_getlong(argv+0) > 0 ) // positive first arg?
    
    // CLOCKS
    x->retry_clock = clock_new((t_object *)x, (method)nocturn_start_low);
    x->outlet_clock = clock_new((t_object *)x, (method)nocturn_feed_outlets);
    
    // OUTLETS, reverse order is important
    x->outf = intout(x);
    x->oute = listout((t_object *)x);
    x->outd = listout((t_object *)x);
    x->outc = listout((t_object *)x);
	x->outb = intout(x);
    x->out = listout((t_object *)x);
    
    // THREADING
    //x->x_qelem = qelem_new(x,(method)simplethread_qfn);
    x->readthread = NULL;
    x->writethread = NULL;
    systhread_mutex_new(&x->read_mutex,0);
    systhread_mutex_new(&x->write_mutex,0);
    x->sleeptime=1;
    x->x_send = 0;
    x->x_start = 0;
    x->retry_warning_pushed = false;
    
    for (i=0;i<8;i++) // 8x8 zeros
    {
        for (j=0;j<8;j++)
        {
            x->dataIn[i][j] = 0;
        }
    }
    
    nocturn_start(x);
    
    
    return (x);
}






void nocturn_assist(t_nocturn *x, void *b, long msg, long arg, char *dst)
{
    if (msg==1) {     // Inlets
        switch (arg) {
            case 0: strcpy(dst, "send [int int], maxwindow 1/0, state 1/0"); break;
            //default: strcpy(dst, "(signal) Audio Input"); break;
        }
    }
    else if (msg==2) { // Outlets
        if (arg == 0)
            strcpy(dst, "Buttons");
        else if (arg == 1)
            strcpy(dst, "Fader");
        else if (arg == 2)
            strcpy(dst, "Rotary Encoders");
        else if (arg == 3)
            strcpy(dst, "Touch Sensors");
        else if (arg == 4)
            strcpy(dst, "?");
        else if (arg == 5)
            strcpy(dst, "Nocturn found");
        else
            strcpy(dst, "?");
    }
}


void nocturn_dblclick(t_nocturn *x)
{
    object_post((t_object *)x, "I got a double-click");
}


void nocturn_acant(t_nocturn *x)
{
    object_post((t_object *)x, "can't touch this!");
}





