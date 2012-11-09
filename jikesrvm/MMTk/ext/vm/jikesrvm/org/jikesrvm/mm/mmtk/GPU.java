/*
 *  This file is part of the Jikes RVM project (http://jikesrvm.org).
 *
 *  This file is licensed to You under the Eclipse Public License (EPL);
 *  You may not use this file except in compliance with the License. You
 *  may obtain a copy of the License at
 *
 *      http://www.opensource.org/licenses/eclipse-1.0.php
 *
 *  See the COPYRIGHT.txt file distributed with this work for information
 *  regarding copyright ownership.
 */
package org.jikesrvm.mm.mmtk;

import org.jikesrvm.classloader.RVMType;
import org.jikesrvm.objectmodel.ObjectModel;
import org.jikesrvm.runtime.SysCall;
import org.mmtk.utility.Log;
import org.mmtk.vm.VM;
import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.Address;
import org.vmmagic.unboxed.AddressArray;
import org.vmmagic.unboxed.ObjectReference;

/**
 * Debugger support for the MMTk harness
 */
@Uninterruptible
public final class GPU extends org.mmtk.vm.GPU {

  @Inline
  @Override
  public int getObjectNumRefs(ObjectReference ref) {
    Object obj = ref.toObject();
    RVMType type = ObjectModel.getObjectType(obj);
    if (type.isClassType())
      return type.asClass().getReferenceOffsets().length;
    if (type.isArrayType() && type.asArray().getElementType().isReferenceType())
      return ObjectModel.getArrayLength(obj);
    return 0;
  }

  @Inline
  @Override  
  public int getObjectRefIndex(ObjectReference ref, Address slot) {
    RVMType type = ObjectModel.getObjectType(ref.toObject());
    int offset = slot.diff(ref.toAddress()).toInt();
    if (type.isClassType()) {
      int[] refOffsets = type.asClass().getReferenceOffsets();
      for (int i = 0; i < refOffsets.length; i++)
        if (refOffsets[i] == offset)
          return i;
    } else if (type.isArrayType()) {
      return offset >> VM.LOG_BYTES_IN_ADDRESS;
    }
    VM.assertions.fail("invalid object reference offset");
    return 0; // not reached, required for javac
  }

  @Override
  public void traceQueue(Address head, Address tail, Address start, Address end) {
    SysCall.sysCall.sysGPUGCtraceQueue(head, tail, start, end);
  }
  
  @Override
  public void traceArray(AddressArray array, int count, Address start, Address end) {
    Address head = ObjectReference.fromObject(array).toAddress();
    Address tail = head.plus(count << VM.LOG_BYTES_IN_ADDRESS);
    SysCall.sysCall.sysGPUGCtraceQueue(head, tail, start, end);
  }

  @Override
  public void harnessBegin() {
    SysCall.sysCall.sysGPUGCreset();
  }

}
