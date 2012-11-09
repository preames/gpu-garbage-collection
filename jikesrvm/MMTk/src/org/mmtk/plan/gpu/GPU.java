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

import org.mmtk.plan.Phase;
import org.mmtk.plan.Plan;
import org.mmtk.plan.StopTheWorld;
import org.mmtk.plan.Trace;
import org.mmtk.plan.TransitiveClosure;
import org.mmtk.policy.MarkSweepSpace;
import org.mmtk.policy.RawPageSpace;
import org.mmtk.policy.Space;
import org.mmtk.utility.Log;
import org.mmtk.utility.deque.AddressDeque;
import org.mmtk.utility.deque.SharedDeque;
import org.mmtk.utility.heap.Mmapper;
import org.mmtk.utility.heap.VMRequest;
import org.mmtk.utility.options.Options;
import org.mmtk.vm.Lock;
import org.mmtk.vm.VM;
import org.vmmagic.pragma.Inline;
import org.vmmagic.pragma.Interruptible;
import org.vmmagic.pragma.Uninterruptible;
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
public class GPU extends StopTheWorld {

  /****************************************************************************
   * Additional phases for the GPU plan.
   */
  public static final short PRE_CLOSURE = Phase.createSimple("gpu-pre-closure");
  public static final short GPU_REF_GRAPH_FILL = Phase.createSimple("gpu-ref-graph-fill");
  public static final short GPU = Phase.createSimple("gpu");
  public static final short GPU_REF_GRAPH_PROCESS = Phase.createSimple("gpu-ref-graph-process");
  public static final short POST_CLOSURE = Phase.createSimple("gpu-post-closure");
  
  // Aligns a pointer to the next starting point for a reference graph entry.
  protected static Address alignRefSpacePtr(Address ptr) {
	  int offset = ptr.toInt() - gpuRefSpace.getStart().toInt();
	  if (offset % (4*4) == (3*4))
		  return ptr;
	  else
		  return ptr.plus((3*4) - (offset % (4*4))); //Address.fromIntZeroExtend((offset & (~0x3)) + 0x4 + 0x3);
  }
  
  protected static final short gpuClosurePhase = Phase.createComplex("gpu-closure", null,
	      Phase.scheduleGlobal     (PRE_CLOSURE),
	      Phase.scheduleCollector  (PRE_CLOSURE),
	      Phase.scheduleGlobal     (GPU_REF_GRAPH_FILL),
	      Phase.scheduleGlobal     (GPU),
	      Phase.scheduleGlobal     (GPU_REF_GRAPH_PROCESS),
	      Phase.scheduleGlobal     (POST_CLOSURE),
	      Phase.scheduleCollector  (POST_CLOSURE));
  
  protected static final short gpuRootClosurePhase = Phase.createComplex("initial-closure", null,
	      Phase.scheduleMutator    (PREPARE),
	      Phase.scheduleGlobal     (PREPARE),
	      Phase.scheduleCollector  (PREPARE),
	      Phase.scheduleComplex    (prepareStacks),
	      Phase.scheduleCollector  (PRECOPY),
	      Phase.scheduleCollector  (STACK_ROOTS),
	      Phase.scheduleCollector  (ROOTS),
	      Phase.scheduleGlobal     (ROOTS),
	      Phase.scheduleComplex    (gpuClosurePhase));
  
  protected static final AddressArray gpuAddresses = AddressArray.create(100000);

  // Reference graph
  public static final Space gpuRefSpace = new RawPageSpace("gpu-refs", 0, VMRequest.create(256, false));
  public static final Lock gpuRefSpaceLock = VM.newLock("gpu-refs");
  public static Address gpuRefSpacePtr = Address.zero();

  @Inline
  public static void allocateRefSpaceNode(ObjectReference ref) {
    int numRefs = VM.gpu.getObjectNumRefs(ref);

    gpuRefSpaceLock.acquire();
    Address node = alignRefSpacePtr(gpuRefSpacePtr);
    gpuRefSpacePtr = node.plus((2 + numRefs) * BYTES_IN_ADDRESS);
    gpuRefSpaceLock.release();

    ref.toAddress().store(node, VM.objectModel.GC_HEADER_OFFSET());
    node.store(ref);
    node.store(numRefs, Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
  }

  @Uninterruptible
  protected static class FillOutRefs extends TransitiveClosure {
    private Address curEdge;
    @Override
    public void processEdge(ObjectReference source, Address slot) {
      ObjectReference dest = slot.loadObjectReference();
      Address destNode = Address.zero();
      if (!dest.isNull()) {
        destNode = dest.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET());
      }
      curEdge.store(destNode);
      curEdge = curEdge.plus(BYTES_IN_ADDRESS);
    }
    @Inline
    public void run() {
      Address curNode = alignRefSpacePtr(gpuRefSpace.getStart());
      while (curNode.LT(gpuRefSpacePtr)) {
        ObjectReference obj = curNode.loadObjectReference();
        int numRefs = curNode.loadInt(Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
        curEdge = curNode.plus(2 * BYTES_IN_ADDRESS);
        VM.scanning.scanObject(this, obj);
        curNode = alignRefSpacePtr(curNode.plus((2 + numRefs) * BYTES_IN_ADDRESS));
        if (VM.VERIFY_ASSERTIONS)
          VM.assertions._assert(alignRefSpacePtr(curEdge).EQ(curNode));
      }
    }
  }
  protected static final FillOutRefs gpuRefSpaceFiller = new FillOutRefs();

  @Uninterruptible
  protected static class MarkObjects extends TransitiveClosure {
    @Inline
    public void processNode(ObjectReference object) {
      // Do nothing, tracing was already done
    }
    @Inline
    public ObjectReference traceObject(ObjectReference object) {
      if (Space.isInSpace(MARK_SWEEP, object))
        return msSpace.traceObject(this, object);
      if (Space.isInSpace(Plan.IMMORTAL, object))
        return Plan.immortalSpace.traceObject(this, object);
      if (Space.isInSpace(Plan.LOS, object))
        return Plan.loSpace.traceObject(this, object);
      if (Space.isInSpace(Plan.NON_MOVING, object))
        return Plan.nonMovingSpace.traceObject(this, object);
      if (Plan.USE_CODE_SPACE && Space.isInSpace(Plan.SMALL_CODE, object))
        return Plan.smallCodeSpace.traceObject(this, object);
      if (Plan.USE_CODE_SPACE && Space.isInSpace(Plan.LARGE_CODE, object))
        return Plan.largeCodeSpace.traceObject(this, object);
      if (VM.VERIFY_ASSERTIONS) {
	  //        Log.write("Failing object => "); Log.writeln(object);
        Space.printVMMap();
        VM.assertions._assert(false, "No special case for space in traceObject");
      }
      return ObjectReference.nullReference();
    }
  }
  protected static final MarkObjects gpuMarker = new MarkObjects();

  public GPU() {
  	super();
  	
    // Collector threads collect all pointers into GPU space locally, then push them
  	// to the shared gpuQueue when finished. GPU (global) pulls from gpuQueue during
  	// the global GPU collection phase.
  	gpuQueue.prepareNonBlocking();
  	
	  collection = Phase.createComplex("collection", null,
		      Phase.scheduleComplex(initPhase),
		      Phase.scheduleComplex(gpuRootClosurePhase),
		      //Phase.scheduleComplex(refTypeClosurePhase),
		      //Phase.scheduleComplex(forwardPhase),
		      Phase.scheduleComplex(completeClosurePhase),
		      Phase.scheduleComplex(finishPhase)
		      );
  }
  
  /****************************************************************************
   * Class variables
   */
  public static final MarkSweepSpace msSpace = new MarkSweepSpace("ms", DEFAULT_POLL_FREQUENCY, VMRequest.create());
  public static final int MARK_SWEEP = msSpace.getDescriptor();

  public static final int SCAN_MARK = 0;

  /****************************************************************************
   * Instance variables
   */
  public final Trace msTrace = new Trace(metaDataSpace);
  public final SharedDeque gpuQueue = new SharedDeque("gpuQueue",metaDataSpace, 1);
  public final AddressDeque myQueue = new AddressDeque("global gpu queue", gpuQueue);
  

  /*****************************************************************************
   * Collection
   */

  /**
   * Perform a (global) collection phase.
   *
   * @param phaseId Collection phase to execute.
   */
  @Inline
  @Override
  public void collectionPhase(short phaseId) {
    if (phaseId == PREPARE) {
      super.collectionPhase(phaseId);
      msTrace.prepare();
      msSpace.prepare(true);
      return;
    }

    if (phaseId == PRE_CLOSURE) {
	//Log.writeln("[GPUGC] Pre-closure (global)...");
      msTrace.prepare();
      return;
    }
    
    if (phaseId == GPU_REF_GRAPH_FILL) {
      gpuRefSpaceFiller.run();
      return;
    }


    if (phaseId == GPU) {
      int count = 0;
      while (myQueue.isNonEmpty())
        gpuAddresses.set(count++, myQueue.pop());
      //      Log.write("Performing GPU GC, root set size = ");
      //Log.writeln(count);
      VM.gpu.traceArray(gpuAddresses, count, gpuRefSpace.getStart(), gpuRefSpacePtr);
      return;
    }

    if (phaseId == GPU_REF_GRAPH_PROCESS) {
      // Transfer marks from reference graph to objects, while compacting the reference graph
      Address from = alignRefSpacePtr(gpuRefSpace.getStart());
      Address to = from;
      while (from.LT(gpuRefSpacePtr)) {
    	//Log.write("Write back ");
    	//Log.write(from);
    	//Log.write(" -> ");
    	//Log.write(to);
    	//Log.write(": ");
        int numRefs = from.loadInt(Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
        //Log.writeln(numRefs);
        if (numRefs < 0) { // Object is reachable
          numRefs &= 0x3FFFFFFF;
          ObjectReference obj = from.loadObjectReference();
          obj.toAddress().store(to, VM.objectModel.GC_HEADER_OFFSET());
          to.store(obj);
          to.store(numRefs, Offset.fromIntZeroExtend(BYTES_IN_ADDRESS));
          to = alignRefSpacePtr(to.plus((2 + numRefs) * BYTES_IN_ADDRESS));
          gpuMarker.traceObject(obj);
        }
        from = alignRefSpacePtr(from.plus((2 + numRefs) * BYTES_IN_ADDRESS));
      }

      //Log.write(" Done, old end = ");
      //Log.write(from);
      //Log.write(", new end = ");
      //Log.writeln(to);
      gpuRefSpacePtr = to;

      return;
    }
    
    if (phaseId == POST_CLOSURE) {
	//      Log.writeln("[GPUGC] Post-closure (global)...");
      msTrace.prepare();
      return;
    }
    
    if (phaseId == CLOSURE) {
	//    	Log.writeln("[GPUGC] Regular closure (global)...");
      msTrace.prepare();
      return;
    }
    
    if (phaseId == RELEASE) {
      msTrace.release();
      msSpace.release();
      super.collectionPhase(phaseId);
      return;
    }

    super.collectionPhase(phaseId);
  }

  /*****************************************************************************
   * Accounting
   */

  /**
   * Return the number of pages reserved for use given the pending
   * allocation.  The superclass accounts for its spaces, we just
   * augment this with the mark-sweep space's contribution.
   *
   * @return The number of pages reserved given the pending
   * allocation, excluding space reserved for copying.
   */
  @Override
  public int getPagesUsed() {
    return (msSpace.reservedPages() + super.getPagesUsed());
  }

  /**
   * Calculate the number of pages a collection is required to free to satisfy
   * outstanding allocation requests.
   *
   * @return the number of pages a collection is required to free to satisfy
   * outstanding allocation requests.
   */
  @Override
  public int getPagesRequired() {
    return super.getPagesRequired() + msSpace.requiredPages();
  }


  /*****************************************************************************
   * Miscellaneous
   */

  @Interruptible
  @Override
  public void boot() {
    super.boot();
    //It would be nice if we could replace this demand mapped region with
    // a normal malloc call since that would speed up the movement of memory
    // to the GC space, but I don't see any easy way to do that within jikes.
    gpuRefSpacePtr = gpuRefSpace.getStart();
    VM.memory.dzmmap(gpuRefSpacePtr, gpuRefSpace.getExtent().toInt());
  }

  @Interruptible
  @Override
  public void postBoot() {
    super.postBoot();
    if (!Options.noReferenceTypes.getValue()) {
	Log.writeln("should use -X:gc:noReferenceTypes=true");
    }
  }

  /**
   * @see org.mmtk.plan.Plan#willNeverMove
   *
   * @param object Object in question
   * @return True if the object will never move
   */
  @Override
  public boolean willNeverMove(ObjectReference object) {
    if (Space.isInSpace(MARK_SWEEP, object))
      return true;
    return super.willNeverMove(object);
  }

  /**
   * Register specialized methods.
   */
  @Interruptible
  @Override
  protected void registerSpecializedMethods() {
    TransitiveClosure.registerSpecializedScan(SCAN_MARK, GPUTraceLocal.class);
    super.registerSpecializedMethods();
  }

  @Interruptible
  public void planHarnessBegin() {
      //run the gc, setup jikes reporting, etc..
      super.planHarnessBegin();

      VM.gpu.harnessBegin();
  }
}
