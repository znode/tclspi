/*
 * tclspi
 */

#include <tcl.h>
#include "tclspi.h"
#include <string.h>


/*
 *--------------------------------------------------------------
 *
 * tclspi_spiObjectDelete -- command deletion callback routine.
 *
 * Results:
 *      ...frees the tclspi handle if it exists.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */
void
tclspi_spiObjectDelete (ClientData clientData)
{
  tclspi_clientData *spiData = (tclspi_clientData *)clientData;

  // free any subordinate structures here

  ckfree((char *)spiData);
}


/*
 *----------------------------------------------------------------------
 *
 * tclspi_spiObjectObjCmd --
 *
 *    dispatches the subcommands of a spi object command
 *
 * Results:
 *    stuff
 *
 *----------------------------------------------------------------------
 */
int
tclspi_spiObjectObjCmd(ClientData cData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int         optIndex;
  tclspi_clientData *spiData = (tclspi_clientData *)cData;

  static CONST char *options[] = {
    "read",
    "write",
    "read_mode",
    "write_mode",
    "read_bits_word",
    "write_bits_word",
    "read_maxspeed",
    "write_maxspeed",
    "delete",
    NULL
  };

  enum options {
    OPT_READ,
    OPT_WRITE,
    OPT_READ_MODE,
    OPT_WRITE_MODE,
    OPT_READ_BITS_WORD,
    OPT_WRITE_BITS_WORD,
    OPT_READ_MAXSPEED,
    OPT_WRITE_MAXSPEED,
    OPT_DELETE,
  };

  /* basic validation of command line arguments */
  if (objc < 2) {
    Tcl_WrongNumArgs (interp, 1, objv, "option ...");
    return TCL_ERROR;
  }

  /* convert the option name to an enum index -- if we get an error,
   * we're done
   */
  if (Tcl_GetIndexFromObj (interp, objv[1], options, "option",
                           TCL_EXACT, &optIndex) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum options) optIndex) {
  case OPT_READ: {
    struct spi_ioc_transfer wspi, rspi;
    unsigned char *addr,*writeBuffer;
    unsigned char *readBuffer;
    int delayUsecs;
    int ret;
    int length;

    // obj transfer data delay
    if (objc != 4) {
      Tcl_WrongNumArgs (interp, 2, objv, "writeData delayUsecs");
      return TCL_ERROR;
    }

    if (spiData->readSpeed == 0) {
      Tcl_AppendResult (interp, "read speed_hz not set", NULL);
      return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj (interp, objv[3], &delayUsecs) == TCL_ERROR) {
      Tcl_AppendResult (interp, " while converting delay microseconds", NULL);
      return TCL_ERROR;
    }

    //length in bytes
    addr = Tcl_GetByteArrayFromObj (objv[2], &length);
    writeBuffer = (unsigned char *)ckalloc (length*4);
    readBuffer = (unsigned char *)ckalloc (length*4);
    memset(writeBuffer, 0, length*4);
    memcpy(writeBuffer, addr, length*2);
    memset(readBuffer, 0, length*4);
    memset(&wspi, 0, sizeof wspi);
    memset(&rspi, 0, sizeof rspi);
    //write Addr+Pads
    wspi.tx_buf = (unsigned long) writeBuffer;
    wspi.rx_buf = 0;
    wspi.len = length*2;
    wspi.delay_usecs = delayUsecs;
    wspi.pad = 0;
    wspi.cs_change = 0;

    //return Addr+Data
    rspi.tx_buf = 0;
    rspi.rx_buf = (unsigned long) readBuffer;
    rspi.len = length*2;
    rspi.delay_usecs = delayUsecs;
    rspi.pad = 0;
    rspi.cs_change = 0;

    // fill in speed and bits per word from the clientData
    // structure (previously set) for write
    // NB could do more here, like allow them to be set
    // in the request
    wspi.speed_hz = spiData->writeSpeed;
    rspi.speed_hz = spiData->readSpeed;

    if (spiData->writeBits == 0) {
      wspi.bits_per_word = 8;
      rspi.bits_per_word = 8;
    } else {
      wspi.bits_per_word = spiData->writeBits;
      rspi.bits_per_word = spiData->readBits;
    }

    ret = ioctl (spiData->fd, SPI_IOC_MESSAGE(1), &wspi);
    ret = ioctl (spiData->fd, SPI_IOC_MESSAGE(1), &rspi);
    if (ret < 0) {
      Tcl_AppendResult (interp, "can't perform spi transfer: ", Tcl_PosixError (interp), NULL);
      ckfree((char *)readBuffer);
      ckfree((char *)writeBuffer);
      return TCL_ERROR;
    }

    Tcl_SetObjResult (interp, Tcl_NewByteArrayObj (readBuffer, length*2));
    ckfree((char *)readBuffer);
    ckfree((char *)writeBuffer);

    return TCL_OK;
  }
  case OPT_WRITE: {
    struct spi_ioc_transfer spi;
    unsigned char *writeBuffer;
    int delayUsecs;
    int ret;
    int length;

    // obj transfer data delay
    if (objc != 4) {
      Tcl_WrongNumArgs (interp, 2, objv, "writeData delayUsecs");
      return TCL_ERROR;
    }

    if (spiData->writeSpeed == 0) {
      Tcl_AppendResult (interp, "write speed_hz not set", NULL);
      return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj (interp, objv[3], &delayUsecs) == TCL_ERROR) {
      Tcl_AppendResult (interp, " while converting delay microseconds", NULL);
      return TCL_ERROR;
    }

    writeBuffer = Tcl_GetByteArrayFromObj (objv[2], &length);
    memset(&spi, 0, sizeof spi);

    spi.tx_buf = (unsigned long) writeBuffer;
    spi.rx_buf = 0;
    spi.len = length;
    spi.delay_usecs = delayUsecs;
    spi.pad = 0;
    spi.cs_change = 0;

    // fill in speed and bits per word from the clientData
    // structure (previously set) for write
    // NB could do more here, like allow them to be set
    // in the request
    spi.speed_hz = spiData->writeSpeed;

    if (spiData->writeBits == 0) {
      spi.bits_per_word = 8;
    } else {
      spi.bits_per_word = spiData->writeBits;
    }

    ret = ioctl (spiData->fd, SPI_IOC_MESSAGE(1), &spi);
    if (ret < 0) {
      Tcl_AppendResult (interp, "can't perform spi transfer: ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
  }

  case OPT_READ_MODE:
  case OPT_WRITE_MODE: {
    int request;
    int modeInt;
    uint8_t mode;
    int ret;

    if (objc != 3) {
      Tcl_WrongNumArgs (interp, 2, objv, "mode");
      return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj (interp, objv[2], &modeInt) == TCL_ERROR) {
      return TCL_ERROR;
    }

    switch (modeInt) {
    case 0:
      mode = SPI_MODE_0;
      break;
    case 1:
      mode = SPI_MODE_1;
      break;
    case 2:
      mode = SPI_MODE_2;
      break;
    case 3:
      mode = SPI_MODE_3;
      break;
    default:
      Tcl_AppendResult (interp, "mode must be 0, 1, 2 or 3", NULL);
      return TCL_ERROR;
    }

    if (optIndex == OPT_READ_MODE) {
      request = SPI_IOC_RD_MODE;
      spiData->readMode = mode;
    } else {
      request = SPI_IOC_WR_MODE;
      spiData->writeMode = mode;
    }

    mode = modeInt;
    ret = ioctl (spiData->fd, request, &mode);
    if (ret < 0) {
      Tcl_AppendResult (interp, "can't set spi mode: ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
  }

  case OPT_READ_BITS_WORD:
  case OPT_WRITE_BITS_WORD: {
    int request;
    int bitsInt;
    uint8_t bits;
    int ret;

    if (objc != 3) {
      Tcl_WrongNumArgs (interp, 2, objv, "bits_per_word");
      return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj (interp, objv[2], &bitsInt) == TCL_ERROR) {
      return TCL_ERROR;
    }

    if (optIndex == OPT_READ_BITS_WORD) {
      request = SPI_IOC_RD_BITS_PER_WORD;
      spiData->readBits = bitsInt;
    } else {
      request = SPI_IOC_WR_BITS_PER_WORD;
      spiData->writeBits = bitsInt;
    }

    bits = bitsInt;
    ret = ioctl (spiData->fd, request, &bits);
    if (ret < 0) {
      Tcl_AppendResult (interp, "can't set spi bits-per-word: ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
  }

  case OPT_READ_MAXSPEED:
  case OPT_WRITE_MAXSPEED: {
    int request;
    int speedInt;
    uint32_t speed;
    int ret;

    if (objc != 3) {
      Tcl_WrongNumArgs (interp, 2, objv, "max_speed_hz");
      return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj (interp, objv[2], &speedInt) == TCL_ERROR) {
      return TCL_ERROR;
    }

    if (optIndex == OPT_READ_MAXSPEED) {
      request = SPI_IOC_RD_MAX_SPEED_HZ;
      spiData->readSpeed = speedInt;
    } else {
      request = SPI_IOC_WR_MAX_SPEED_HZ;
      spiData->writeSpeed = speedInt;
    }

    speed = speedInt;
    ret = ioctl (spiData->fd, request, &speed);
    if (ret < 0) {
      Tcl_AppendResult (interp, "can't set spi speed: ", Tcl_PosixError (interp), NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
  }

  case OPT_DELETE: {
    if (objc != 2) {
      Tcl_WrongNumArgs (interp, 1, objv, "delete");
      return TCL_ERROR;
    }

    close (spiData->fd);
    Tcl_DeleteCommandFromToken(interp, spiData->cmdToken);
    return TCL_OK;
  }
  }

  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * tclspi_spiObjCmd --
 *
 *      This procedure is invoked to process the "spi" command.
 *      See the user documentation for details on what it does.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      See the user documentation.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
int
tclspi_spiObjCmd(clientData, interp, objc, objv)
     ClientData clientData;              /* registered proc hashtable ptr. */
     Tcl_Interp *interp;                 /* Current interpreter. */
     int objc;                           /* Number of arguments. */
Tcl_Obj   *CONST objv[];
{
  tclspi_clientData *spiData;
  char               *commandName;
  int                 autoGeneratedName;

  // basic command line processing
  if (objc != 3) {
    Tcl_WrongNumArgs (interp, 1, objv, "objName spiDevice");
    return TCL_ERROR;
  }

  // allocate one of our spi client data objects for Tcl and configure it
  spiData = (tclspi_clientData *)ckalloc (sizeof (tclspi_clientData));

  // zero out the structure
  memset (spiData, 0, sizeof(tclspi_clientData));

  spiData->fd = open (Tcl_GetString (objv[2]), O_RDWR);
  if (spiData->fd < 0) {
    Tcl_AppendResult (interp, "can't open '", Tcl_GetString (objv[2]), "': ", Tcl_PosixError (interp), NULL);
    return TCL_ERROR;
  }

  commandName = Tcl_GetString (objv[1]);

  // if commandName is #auto, generate a unique name for the object
  autoGeneratedName = 0;
  if (strcmp (commandName, "#auto") == 0) {
    static unsigned long nextAutoCounter = 0;
    char *objName;
    int    baseNameLength;

    objName = Tcl_GetStringFromObj (objv[0], &baseNameLength);
    baseNameLength += 42;
    commandName = ckalloc (baseNameLength);
    snprintf (commandName, baseNameLength, "%s%lu", objName, nextAutoCounter++);
    autoGeneratedName = 1;
  }

  // create a Tcl command to interface to spi
  spiData->cmdToken = Tcl_CreateObjCommand (interp, commandName, tclspi_spiObjectObjCmd, spiData, tclspi_spiObjectDelete);
  Tcl_SetObjResult (interp, Tcl_NewStringObj (commandName, -1));
  if (autoGeneratedName == 1) {
    ckfree(commandName);
  }
  return TCL_OK;
}

