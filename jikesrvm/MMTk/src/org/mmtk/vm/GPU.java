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
package org.mmtk.vm;

import org.vmmagic.pragma.Uninterruptible;
import org.vmmagic.unboxed.Address;
import org.vmmagic.unboxed.AddressArray;
import org.vmmagic.unboxed.ObjectReference;

/**
 * Methods for doing GC on a GPU
 */
@Uninterruptible
public abstract class GPU {

  public abstract int getObjectNumRefs(ObjectReference ref);

  public abstract int getObjectRefIndex(ObjectReference ref, Address slot);

  public abstract void traceQueue(Address head, Address tail, Address start, Address end);
  
  public abstract void traceArray(AddressArray array, int count, Address start, Address end);

  public abstract void harnessBegin();
}
