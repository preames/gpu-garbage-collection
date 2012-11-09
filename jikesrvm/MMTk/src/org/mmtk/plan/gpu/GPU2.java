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

import org.mmtk.plan.TransitiveClosure;
import org.mmtk.utility.Log;
import org.mmtk.utility.options.Options;
import org.mmtk.vm.VM;
import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.*;

/**
 * This class implements the global state of a simple mark-sweep collector.
 *
 * All plans make a clear distinction between <i>global</i> and
 * <i>thread-local</i> activities, and divides global and local state
 * into separate class hierarchies.  Global activities must be
 * synchronized, whereas no synchronization is required for
 * thread-local activities.  There is a single instance of Plan (or the
 * appropriate sub-class), and a 1:1 mapping of PlanLocal to "kernel
 * threads" (aka CPUs).  Thus instance
 * methods of PlanLocal allow fast, unsychronized access to functions such as
 * allocation and collection.
 *
 * The global instance defines and manages static resources
 * (such as memory and virtual memory resources).  This mapping of threads to
 * instances is crucial to understanding the correctness and
 * performance properties of MMTk plans.
 */
@Uninterruptible
public class GPU2 extends GPU {

  @Uninterruptible
  protected static class VerifyRefs extends TransitiveClosure {
    /* Verify that the reference graph is consistent with the actual heap */
    private Address curEdge;
    @Override
    public void processEdge(ObjectReference source, Address slot) {
      ObjectReference dest = slot.loadObjectReference();
      Address destNode = Address.zero();
      if (!dest.isNull()) {
        destNode = dest.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET());
      }
      VM.assertions._assert(curEdge.loadAddress().EQ(destNode));
      curEdge = curEdge.plus(BYTES_IN_ADDRESS);
    }
    @Inline
    public void run() {
      Address curNode, nextNode;
      for (curNode = gpuRefSpace.getStart(); curNode.LT(gpuRefSpacePtr); curNode = nextNode) {
        ObjectReference obj = curNode.loadObjectReference();
        int numRefs = curNode.loadInt(Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
        nextNode = curNode.plus((2 + numRefs) * BYTES_IN_ADDRESS);
        if (!obj.isNull()) {
          VM.assertions._assert(curNode.EQ(obj.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET())));
          curEdge = curNode.plus(2 * BYTES_IN_ADDRESS);
          VM.scanning.scanObject(this, obj);
          VM.assertions._assert(curEdge.EQ(nextNode));
        }
      }
    }
  }
  protected static final VerifyRefs gpuRefSpaceVerifier = new VerifyRefs();

  @Inline
  @Override
  public void collectionPhase(short phaseId) {
    if (phaseId == GPU_REF_GRAPH_FILL) {
      // Don't need to fill out edges, but optionally verify for debugging purposes
      if (Options.sanityCheck.getValue()) {
        Log.writeln("Verifying reference graph");
        gpuRefSpaceVerifier.run();
      }
      return;
    }

    if (phaseId == GPU_REF_GRAPH_PROCESS) {
      // TODO: compaction while preserving edges is expensive, but we can't just
      // let the graph grow forever...
      Address from = gpuRefSpace.getStart();
      while (from.LT(gpuRefSpacePtr)) {
        int numRefs = from.loadInt(Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
        if (numRefs < 0) { // Object is reachable
          numRefs &= 0x3FFFFFFF;
          from.store(numRefs, Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
          gpuMarker.traceObject(from.loadObjectReference());
        } else {
          from.store(ObjectReference.nullReference());
        }
        from = from.plus((2 + numRefs) * BYTES_IN_ADDRESS);
      }
      return;
    }

    super.collectionPhase(phaseId);
  }
}
