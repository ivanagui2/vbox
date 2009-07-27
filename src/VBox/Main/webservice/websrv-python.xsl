<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:vbox="http://www.virtualbox.org/">

<!--

    websrv-python.xsl:
        XSLT stylesheet that generates VirtualBox_services.py from
        VirtualBox.xidl. This Python file represents our
        web service API. Depends on WSDL file for actual SOAP bindings.

     Copyright (C) 2008 Sun Microsystems, Inc.

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

     Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
     Clara, CA 95054 USA or visit http://www.sun.com if you need
     additional information or have any questions.
-->


<xsl:output
  method="text"
  version="1.0"
  encoding="utf-8"
  indent="no"/>

<xsl:include href="websrv-shared.inc.xsl" />

<xsl:variable name="G_setSuppressedInterfaces"
              select="//interface[@wsmap='suppress']" />

<xsl:template name="emitConvertedType">
  <xsl:param name="ifname" />
  <xsl:param name="methodname" />
  <xsl:param name="type" />
  <xsl:choose>
    <xsl:when test="$type='wstring'">String</xsl:when>
    <xsl:when test="$type='boolean'">Boolean</xsl:when>
    <xsl:when test="$type='unsigned long'">UnsignedInt</xsl:when>
    <xsl:when test="$type='double'">Double</xsl:when>
    <xsl:when test="$type='float'">Float</xsl:when>
    <xsl:when test="$type='long'">Int</xsl:when>
    <xsl:when test="$type='long long'">Long</xsl:when>
    <xsl:when test="$type='short'">Short</xsl:when>
    <xsl:when test="$type='unsigned short'">UnsignedShort</xsl:when>
    <xsl:when test="$type='unsigned long long'">UnsignedLong</xsl:when>
    <xsl:when test="$type='result'">UnsignedInt</xsl:when>
    <xsl:when test="$type='$unknown'">IUnknown</xsl:when>
    <xsl:when test="$type='$dispatched'">IUnknown</xsl:when>
    <xsl:otherwise><xsl:value-of select="$type" /></xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="emitOutParam">
  <xsl:param name="ifname" />
  <xsl:param name="methodname" />
  <xsl:param name="type" />
  <xsl:param name="value" />
  <xsl:param name="safearray" />
  
  <xsl:call-template name="emitConvertedType">
    <xsl:with-param name="ifname" select="$ifname" />
    <xsl:with-param name="methodname" select="$methodname" />
    <xsl:with-param name="type" select="$type" />
  </xsl:call-template>
  <xsl:text>(</xsl:text>
   <xsl:text>self.mgr,</xsl:text>
  <xsl:value-of select="$value"/>
  <xsl:if test="$safearray='yes'">
    <xsl:value-of select="', True'"/>
  </xsl:if>
  <xsl:text>)</xsl:text>
</xsl:template>


<xsl:template name="emitGetAttribute">
  <xsl:param name="ifname" />
  <xsl:param name="attrname" />
  <xsl:param name="attrtype" />
  <xsl:param name="attrsafearray" />
  <xsl:variable name="fname"><xsl:call-template name="makeGetterName"><xsl:with-param name="attrname" select="$attrname"/></xsl:call-template> </xsl:variable>
   def <xsl:value-of select="$fname"/>(self):
       req=<xsl:value-of select="$ifname"/>_<xsl:value-of select="$fname"/>RequestMsg()
       req._this=self.handle
       val=self.mgr.getPort().<xsl:value-of select="$ifname"/>_<xsl:value-of select="$fname"/>(req)
       <xsl:text>return  </xsl:text>
       <xsl:call-template name="emitOutParam">
           <xsl:with-param name="ifname" select="$ifname" />
           <xsl:with-param name="methodname" select="@name" />
           <xsl:with-param name="type" select="$attrtype" />
           <xsl:with-param name="value" select="concat('val.','_returnval')" />
           <xsl:with-param name="safearray" select="@safearray"/>
         </xsl:call-template>      
</xsl:template>

<xsl:template name="emitSetAttribute">
  <xsl:param name="ifname" />
  <xsl:param name="attrname" />
  <xsl:param name="attrtype" />
  <xsl:param name="attrsafearray" />
  <xsl:variable name="fname"><xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="$attrname"/></xsl:call-template> </xsl:variable>
   def <xsl:value-of select="$fname"/>(self, value):
       req=<xsl:value-of select="$ifname"/>_<xsl:value-of select="$fname"/>RequestMsg()
       req._this=self.handle
       if type(value) in [int, bool, basestring]:
            req._<xsl:value-of select="$attrname"/> = value
       else:
            req._<xsl:value-of select="$attrname"/> = value.handle
       self.mgr.getPort().<xsl:value-of select="$ifname"/>_<xsl:value-of select="$fname"/>(req)      
</xsl:template>

<xsl:template name="collection">
   <xsl:variable name="cname"><xsl:value-of select="@name" /></xsl:variable>
   <xsl:variable name="ename"><xsl:value-of select="@type" /></xsl:variable>
class <xsl:value-of select="$cname"/>:
   def __init__(self, mgr, array):
       self.array = array
       self.mgr = mgr

   def __next(self):
       return self.array.__next()

   def __size(self):
       return self.array._array.__size()

   def __len__(self):
       return self.array._array.__len__()

   def __getitem__(self, index):
       return <xsl:value-of select="$ename"/>(self.mgr, self.array._array[index])
       
</xsl:template>


<xsl:template name="computeExtends">
  <xsl:param name="base" />
  
  <xsl:choose>
    <xsl:when test="($base = '$unknown') or ($base = '$dispatched')">
      <xsl:value-of select="'IUnknown'"/>
    </xsl:when>
     <xsl:otherwise>
      <xsl:value-of select="$base"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="interface">
   <xsl:variable name="base">
     <xsl:call-template name="computeExtends">
       <xsl:with-param name="base" select="@extends" />
     </xsl:call-template>
   </xsl:variable>
   <xsl:variable name="ifname"><xsl:value-of select="@name" /></xsl:variable>

class <xsl:value-of select="$ifname"/>(<xsl:value-of select="$base" />):
   def __init__(self, mgr, handle, isarray = False):
       self.mgr = mgr
       if handle is None:
           raise Exception("bad handle: "+str(handle))
       self.handle = handle
       self.isarray = isarray
<!--
    This doesn't work now
       mgr.register(handle)

   def __del__(self):
       mgr.unregister(self.handle) 
-->
   def releaseRemote(self):
        try:
            req=IManagedObjectRef_releaseRequestMsg() 
            req._this=handle
            self.mgr.getPort().IManagedObjectRef_release(req)
        except:
            pass

   def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

   def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

   def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

   def __getitem__(self, index):
      if self.isarray:
          return <xsl:value-of select="$ifname" />(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       

   def __str__(self):
        return self.handle
   
   def isValid(self):
        return self.handle != None and self.handle != ''

   def __getattr__(self,name):
      hndl = <xsl:value-of select="$ifname" />._Attrs_.get(name, None)
      if hndl != None:
         if hndl[0] != None:
           return hndl[0](self)
         else:
          raise AttributeError
      else:
         return <xsl:value-of select="$base" />.__getattr__(self, name)

   def __setattr__(self, name, val):
      hndl = <xsl:value-of select="$ifname" />._Attrs_.get(name, None)
      if (hndl != None and hndl[1] != None):
         hndl[1](self,val)
      else:
         self.__dict__[name] = val

   <xsl:for-each select="method">
       <xsl:call-template name="method"/>
  </xsl:for-each>

  <xsl:for-each select="attribute">
      <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
      <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
      <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
      <!-- skip this attribute if it has parameters of a type that has wsmap="suppress" -->
      <xsl:choose>
        <xsl:when test="( $attrtype=($G_setSuppressedInterfaces/@name) )">
          <xsl:comment><xsl:value-of select="concat('skipping attribute ', $attrtype, ' for it is of a suppressed type')" /></xsl:comment>
        </xsl:when>
        <xsl:otherwise>
          <xsl:choose>
            <xsl:when test="@readonly='yes'">
              <xsl:comment> readonly attribute <xsl:copy-of select="$ifname" />::<xsl:copy-of select="$attrname" /> </xsl:comment>
            </xsl:when>
            <xsl:otherwise>
              <xsl:comment> read/write attribute <xsl:copy-of select="$ifname" />::<xsl:copy-of select="$attrname" /> </xsl:comment>
            </xsl:otherwise>
          </xsl:choose>
          <!-- aa) get method: emit request and result -->
          <xsl:call-template name="emitGetAttribute">
            <xsl:with-param name="ifname" select="$ifname" />
            <xsl:with-param name="attrname" select="$attrname" />
            <xsl:with-param name="attrtype" select="$attrtype" />
          </xsl:call-template>
          <!-- bb) emit a set method if the attribute is read/write -->
          <xsl:if test="not($attrreadonly='yes')">
            <xsl:call-template name="emitSetAttribute">
              <xsl:with-param name="ifname" select="$ifname" />
              <xsl:with-param name="attrname" select="$attrname" />
              <xsl:with-param name="attrtype" select="$attrtype" />
            </xsl:call-template>
          </xsl:if>
        </xsl:otherwise>
      </xsl:choose>
  </xsl:for-each>


   _Attrs_=<xsl:text>{</xsl:text>
  <xsl:for-each select="attribute">
    <xsl:if test="not( @type=($G_setSuppressedInterfaces/@name) )">
      <xsl:text>         </xsl:text>'<xsl:value-of select="@name"/>'<xsl:text>:[</xsl:text>
      <xsl:call-template name="makeGetterName">
        <xsl:with-param name="attrname" select="@name"/>
      </xsl:call-template>
      <xsl:text>,</xsl:text>
      <xsl:choose>
        <xsl:when test="@readonly='yes'">
          <xsl:text>None</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="makeSetterName">
            <xsl:with-param name="attrname" select="@name"/>
          </xsl:call-template>,
        </xsl:otherwise>
      </xsl:choose>
      <xsl:text>]</xsl:text>
      <xsl:if test="not(position()=last())"><xsl:text>,&#10;</xsl:text></xsl:if>
      </xsl:if>
  </xsl:for-each>
  <xsl:text>}</xsl:text>
</xsl:template>

<xsl:template name="interfacestruct">
   <xsl:variable name="ifname"><xsl:value-of select="@name" /></xsl:variable>
class <xsl:value-of select="$ifname"/>:
    def __init__(self, mgr, handle, isarray = False):
       self.mgr = mgr
       self.isarray = isarray
       if isarray:
          self.handle = handle
       else:
<xsl:for-each select="attribute">
          self.<xsl:value-of select="@name"/> = <xsl:call-template name="emitConvertedType">
              <xsl:with-param name="ifname" select="$ifname" />
              <xsl:with-param name="methodname" select="''" />
              <xsl:with-param name="type" select="@type" />
             </xsl:call-template>(self.mgr, handle._<xsl:value-of select="@name"/>)
       </xsl:for-each>
          pass

   <!-- also do getters/setters -->
    <xsl:for-each select="attribute">
    def <xsl:call-template name="makeGetterName"><xsl:with-param name="attrname" select="@name"/></xsl:call-template>(self):
       return self.<xsl:value-of select="@name"/>
 
    def <xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="@name"/></xsl:call-template>(self):
       raise Error, 'setters not supported'
    </xsl:for-each>

    def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

    def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

    def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

    def __getitem__(self, index):
      if self.isarray:
          return <xsl:value-of select="$ifname" />(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       

</xsl:template>

<xsl:template name="genreq">
       <xsl:text>req=</xsl:text><xsl:value-of select="../@name"/>_<xsl:value-of select="@name"/>RequestMsg()
       req._this=self.handle
       <xsl:for-each select="param[@dir='in']">
       req._<xsl:value-of select="@name" />=_arg_<xsl:value-of select="@name" />
       </xsl:for-each>
       val=self.mgr.getPort().<xsl:value-of select="../@name"/>_<xsl:value-of select="@name"/>(req)
       <!-- return needs to be the first one -->      
       return <xsl:for-each select="param[@dir='return']">
         <xsl:call-template name="emitOutParam">
           <xsl:with-param name="ifname" select="../@name" />
           <xsl:with-param name="methodname" select="@name" />
           <xsl:with-param name="type" select="@type" />
           <xsl:with-param name="value" select="concat('val.','_returnval')" />
           <xsl:with-param name="safearray" select="@safearray"/>
         </xsl:call-template>         
         <xsl:if test="../param[@dir='out']">
           <xsl:text>, </xsl:text>
         </xsl:if>
       </xsl:for-each>
       <xsl:for-each select="param[@dir='out']">
         <xsl:if test="not(position()=1)">
           <xsl:text>, </xsl:text>
         </xsl:if>
         <xsl:call-template name="emitOutParam">
           <xsl:with-param name="ifname" select="../@name" />
           <xsl:with-param name="methodname" select="@name" />
           <xsl:with-param name="type" select="@type" />
           <xsl:with-param name="value" select="concat('val._',@name)" />
           <xsl:with-param name="safearray" select="@safearray"/>
         </xsl:call-template>
       </xsl:for-each>
       <xsl:text>&#10;&#10;</xsl:text>
</xsl:template>

<xsl:template name="method" > 
   def <xsl:value-of select="@name"/><xsl:text>(self</xsl:text>
   <xsl:for-each select="param[@dir='in']">
     <xsl:text>, </xsl:text>
     <xsl:value-of select="concat('_arg_',@name)"/>
   </xsl:for-each><xsl:text>):&#10;       </xsl:text>
   <xsl:call-template name="genreq"/>
</xsl:template>

<xsl:template name="makeConstantName" >
    <xsl:choose>
      <!-- special case for reserved word, maybe will need more in the future -->
      <xsl:when test="@name='None'">
        <xsl:text>_None</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="@name"/>
      </xsl:otherwise>
    </xsl:choose>   
</xsl:template>

<xsl:template name="enum">
class <xsl:value-of select="@name"/>:
   def __init__(self,mgr,handle):
       self.mgr=mgr
       if isinstance(handle,basestring):
           self.handle=<xsl:value-of select="@name"/>._ValueMap[handle]
       else:
           self.handle=handle

   def __eq__(self,other):
      if isinstance(other,<xsl:value-of select="@name"/>):
         return self.handle == other.handle
      if isinstance(other,int):
         return self.handle == other
      return False

   def __ne__(self,other):
      if isinstance(other,<xsl:value-of select="@name"/>):
         return self.handle != other.handle
      if isinstance(other,int):
         return self.handle != other
      return True

   def __str__(self):
        return <xsl:value-of select="@name"/>._NameMap[self.handle]

   def __int__(self):
        return self.handle

   _NameMap={<xsl:for-each select="const">
              <xsl:value-of select="@value"/>:'<xsl:value-of select="@name"/>'<xsl:if test="not(position()=last())">,</xsl:if>
              </xsl:for-each>}
   _ValueMap={<xsl:for-each select="const">
              '<xsl:value-of select="@name"/>':<xsl:value-of select="@value"/><xsl:if test="not(position()=last())">,</xsl:if>
              </xsl:for-each>}

<xsl:for-each select="const"><xsl:text>   </xsl:text><xsl:call-template name="makeConstantName"><xsl:with-param name="name" select="@name"/></xsl:call-template>=<xsl:value-of select="@value"/><xsl:text>&#xa;</xsl:text>
</xsl:for-each>
</xsl:template>

<xsl:template match="/">
<xsl:text># Copyright (C) 2008-2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#
# This file is autogenerated from VirtualBox.xidl, DO NOT EDIT!
#
from VirtualBox_services import *

class ManagedManager:
  def __init__(self):
     self.map = {}

  def register(self,handle):
     if handle == None:
        return
     c = self.map.get(handle,0)
     c = c + 1
     self.map[handle]=c

  def unregister(self,handle):
     if handle == None:
        return
     c = self.map.get(handle,-1)
     if c == -1:
        raise Error, 'wrong refcount'
     c = c - 1
     if c == 0:
        try:
            req=IManagedObjectRef_releaseRequestMsg() 
            req._this=handle
            self.mgr.getPort().IManagedObjectRef_release(req)
        except:
            pass
        finally:
            self.map[handle] = -1
     else:
        self.map[handle] = c
  
class String:
  def __init__(self, mgr, handle, isarray = False):
      self.handle = handle
      self.mgr = mgr
      self.isarray = isarray
 
  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return String(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       

  def __str__(self):
      return str(self.handle)

  def __eq__(self,other):
      if self.isarray:
         return isinstance(other,String) and self.handle == other.handle
      if isinstance(other,String):
         return self.handle == other.handle
      if isinstance(other,basestring):
         return self.handle == other
      return False

  def __ne__(self,other):
      if self.isarray:
         return not isinstance(other,String) or self.handle == other.handle     
      if isinstance(other,String):
         return self.handle != other.handle
      if isinstance(other,basestring):
         return self.handle != other
      return True

  def __add__(self,other):
      return str(self.handle)+str(other)


class Boolean:
  def __init__(self, mgr, handle, isarray = False):       
       self.handle = handle
       if self.handle == "false":
          self.handle = None
       self.mgr = mgr
       self.isarray = isarray
 
  def __str__(self):
       return "true" if self.handle else "false"

  def __eq__(self,other):
      if isinstance(other,Bool):
         return self.handle == other.value
      if isinstance(other,bool):
         return self.handle == other
      return False

  def __ne__(self,other):
      if isinstance(other,Bool):
         return self.handle != other.handle
      if isinstance(other,bool):
         return self.handle != other
      return True

  def __int__(self):
       return 1 if self.handle else 0

  def __long__(self):
       return 1 if self.handle else 0

  def __nonzero__(self):
       return True if self.handle else False

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return Boolean(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       

class UnsignedInt:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray
 
  def __str__(self):
       return str(self.handle)

  def __int__(self):
       return int(self.handle)

  def __long__(self):
       return long(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return UnsignedInt(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       

    
class Int:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return Int(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       
 
  def __str__(self):
       return str(self.handle)
  
  def __int__(self):
       return int(self.handle)

  def __long__(self):
       return long(self.handle)

class UnsignedShort:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray
 
  def __str__(self):
       return str(self.handle)

  def __int__(self):
       return int(self.handle)

  def __long__(self):
       return long(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return UnsignedShort(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       

class Short:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray
 
  def __str__(self):
       return str(self.handle)
  
  def __int__(self):
       return int(self.handle)

  def __long__(self):
       return long(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return Short(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"

class UnsignedLong:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray
 
  def __str__(self):
       return str(self.handle)

  def __int__(self):
       return int(self.handle)

  def __long__(self):
       return long(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return UnsignedLong(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"

class Long:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray

  def __str__(self):
       return str(self.handle)

  def __int__(self):
       return int(self.handle)

  def __long__(self):
       return int(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return Long(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"

class Double:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray
 
  def __str__(self):
       return str(self.handle)

  def __int__(self):
       return int(self.handle)

  def __float__(self):
       return float(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return Double(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"

class Float:
  def __init__(self, mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr       
       self.isarray = isarray

  def __str__(self):
       return str(self.handle)

  def __int__(self):
       return int(self.handle)

  def __float__(self):
       return float(self.handle)

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return Float(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"

class IUnknown:
  def __init__(self,  mgr, handle, isarray = False):
       self.handle = handle
       self.mgr = mgr
       self.isarray = isarray

  def __nonzero__(self):
       return True if self.handle != "" else False

  def __next(self):
      if self.isarray:
          return self.handle.__next()
      raise TypeError, "iteration over non-sequence"

  def __size(self):
      if self.isarray:         
          return self.handle.__size()
      raise TypeError, "iteration over non-sequence"

  def __len__(self):
      if self.isarray:
          return self.handle.__len__()
      raise TypeError, "iteration over non-sequence"

  def __getitem__(self, index):
      if self.isarray:
          return IUnknown(self.mgr, self.handle[index])
      raise TypeError, "iteration over non-sequence"       
  
  def __str__(self):
       return str(self.handle)

  def __getattr__(self,attr):
       if self.__class__.__dict__.get(attr) != None:
           return self.__class__.__dict__.get(attr)
       if self.__dict__.get(attr) != None:
           return self.__dict__.get(attr)
       raise AttributeError

</xsl:text>
  <xsl:for-each select="//interface[@wsmap='managed' or @wsmap='global']">
       <xsl:call-template name="interface"/>
  </xsl:for-each>
   <xsl:for-each select="//interface[@wsmap='struct']">
       <xsl:call-template name="interfacestruct"/>
  </xsl:for-each>
  <xsl:for-each select="//collection">
       <xsl:call-template name="collection"/>
  </xsl:for-each>
  <xsl:for-each select="//enum">
       <xsl:call-template name="enum"/>
  </xsl:for-each>

class IWebsessionManager2(IWebsessionManager):
  def __init__(self, url):
       self.url = url
       self.port = None
       self.handle = None
       self.mgr = self

  def getPort(self):
      if self.port is None:
          self.port = vboxServiceLocator().getvboxPortType(self.url)
      return self.port

</xsl:template>

</xsl:stylesheet> 
