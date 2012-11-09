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
package org.mmtk.plan.gpu;

import org.mmtk.utility.Log;
import org.mmtk.vm.VM;
import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.*;

/**
 * This class implements <i>per-mutator thread</i> behavior
 * and state for the <i>MS</i> plan, which implements a full-heap
 * mark-sweep collector.<p>
 *
 * Specifically, this class defines <i>MS</i> mutator-time allocation
 * and per-mutator thread collection semantics (flushing and restoring
 * per-mutator allocator state).<p>
 *
 * @see GPU
 * @see GPUCollector
 * @see StopTheWorldMutator
 * @see MutatorContext
 */
@Uninterruptible
public class GPU2Mutator extends GPUMutator {
  //int callcount1 = 0;
  //int callcount2 = 0;

  // The -X:gc:noReferenceTypes=true option makes weak references and such
  // into strong references (traced by GC), but write barriers are still
  // not called for them
  @Inline
  @Override
  public void javaLangReferenceUpdated(ObjectReference src, ObjectReference tgt) {
    Address srcNode = src.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET());
    //VM.assertions._assert(srcNode.loadObjectReference().toAddress().EQ(src.toAddress()));
    // Assuming address of referent is reference object's first reference
    srcNode.store( 
      tgt.isNull() ? Address.zero() : tgt.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET()),
      Offset.fromIntZeroExtend(2 * BYTES_IN_ADDRESS));
    //callcount1++;
  }

  @Inline
  @Override
  public void objectReferenceWrite(ObjectReference src, Address slot, ObjectReference tgt, Word metaDataA, Word metaDataB, int mode) {
    VM.barriers.objectReferenceWrite(src, tgt, metaDataA, metaDataB, mode);
    Address srcNode = src.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET());
    if (!srcNode.isZero()) {
      //VM.assertions._assert(srcNode.loadObjectReference().toAddress().EQ(src.toAddress()));
      srcNode.store( 
        tgt.isNull() ? Address.zero() : tgt.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET()),
        Offset.fromIntZeroExtend((2 + VM.gpu.getObjectRefIndex(src, slot)) * BYTES_IN_ADDRESS));
    }
    //callcount2++;
  }

  @Inline
  @Override
  public void collectionPhase(short phaseId, boolean primary) {
    if (phaseId == GPU.PREPARE) {
      /*Log.write("calls to GPU2Mutator.javaLangReferenceUpdated: ");
      Log.writeln(callcount1);
      Log.write("calls to GPU2Mutator.objectReferenceWrite: ");
      Log.writeln(callcount2);
      callcount1 = 0;
      callcount2 = 0;*/
      super.collectionPhase(phaseId, primary);
      return;
    }
  }

}
