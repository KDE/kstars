#ifndef _PPort_hpp_
#define _PPort_hpp_

class port_t;

/** To access and share the parallel port
    between severa objects. */
class PPort {
public:
   PPort();
   PPort(int ioPort);
   /** set the ioport associated to the // port.
       \return true if the binding was possible
   */
   bool setPort(int ioPort);
   /** set a data bit of the // port.
       \return false if IO port not set, or bit not registered by ID.
       \param ID the identifier used to register the bit
       \param stat the stat wanted for the given bit
       \param bit the bit to set. They are numbered from 0 to 7.

   */
   bool setBit(const void * ID,int bit,bool stat);
   /** register a bit for object ID.
       \param ID the identifier used to register the bit it should be the 'this' pointer.
       \param bit the bit of the // port to register
       \return false if the bit is allready register with an
               other ID, or if the // port is not initialised.
   */
   bool registerBit(const void * ID,int bit);
   /** release a bit.
       \param ID the identifier used to register the bit
       \param bit the bit to register.
       \return false if the bit was not registered by ID or
               if the if the // port is not initialised.
   */
   bool unregisterBit(const void * ID,int bit);

   /** test if a bit is registerd.
       \param ID the identifier used to register the bit
       \param bit the bit to test.
       \return false if the bit was not registered by ID or
               if the if the // port is not initialised.
   */
   bool isRegisterBit(const void * ID,int bit) const;
   
   /** set the bits off the // port according to previous call to setBit().
    */
   bool commit();
private:
   void reset();
   unsigned char bitArray;
   const void * assignedBit[8];
   port_t * currentPort;
};
#endif
