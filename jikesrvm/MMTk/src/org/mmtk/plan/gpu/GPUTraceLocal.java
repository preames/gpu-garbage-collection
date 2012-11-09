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

import org.mmtk.plan.TraceLocal;
import org.mmtk.plan.Trace;
import org.mmtk.policy.Space;
import org.mmtk.utility.HeaderByte;
import org.mmtk.utility.Log;
import org.mmtk.utility.deque.AddressDeque;
import org.mmtk.utility.deque.ObjectReferenceDeque;
import org.mmtk.utility.deque.SharedDeque;
import org.mmtk.vm.VM;

import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.*;

/**
 * This class implements the thread-local functionality for a transitive
 * closure over a mark-sweep space.
 */
@Uninterruptible
public final class GPUTraceLocal extends TraceLocal {
  /****************************************************************************
   * Instance fields
   */
  private final ObjectReferenceDeque modBuffer;
  
  /* objects to be offloaded to the GPU */
  protected final AddressDeque gpuValues;
  
  protected boolean redirectGPUNodes = false;
  
  /**
   * Constructor
   */
  public GPUTraceLocal(Trace trace, ObjectReferenceDeque modBuffer, SharedDeque gpuQueue) {
    super(GPU.SCAN_MARK, trace);
    this.modBuffer = modBuffer;
    this.gpuValues = new AddressDeque("local-gpu-queue", gpuQueue);
  }

  /****************************************************************************
   * Externally visible Object processing and tracing
   */
  
  @Inline
  @Override
  public void processNode(ObjectReference object) {
    VM.assertions.fail("not used");
    values.push(object);
  }

  @Override
  public void prepare() {
  	redirectGPUNodes = true;
  }

  public void preGPUTrace() {
  	super.completeTrace();
  	// Flush all the locally collected nodes in the GPU space to the shared deque.
  	gpuValues.flushLocal();
  }
  
  /**
   * Finishing processing all GC work.  This method iterates until all work queues
   * are empty. This is specific to the GPU collector!!!
   */
  @Inline
  @Override
  public void completeTrace() {
  	// Complete the entire trace.
  	redirectGPUNodes = false;
  	super.completeTrace();
	
	/*
	Log.writeln("Offloading GC trace...");
    if (!rootLocations.isEmpty()) {
      processRoots();
    }
    
    if (!values.getTail().isZero()) {
    	Log.writeln("WARNING: MORE THAN ONE BUFFER (NOT IMPLEMENTED YET)");
    }
    
    // Ok, this is more complex... it's a queue of buffers. But as long as the tail is null,
    // we're fine, since this means it contains only one buffer. Still, should consider to
    // move everything over.
    VM.gpu.traceQueue(values.getBufferStart(), values.getHead());
    
    // What we really want to do here is: We want to do the whole trace and enqueue everything
    // that points into the GPU GC space to a special queue which we then pass to the sys call
    // so that it gets garbage collected.
    
    Log.writeln("-- Finished GPU GC --");*/
  }

  /**
   * Is the specified object live?
   *
   * @param object The object.
   * @return <code>true</code> if the object is live.
   */
  @Override
  public boolean isLive(ObjectReference object) {
    if (object.isNull()) return false;
    if (Space.isInSpace(GPU.MARK_SWEEP, object)) {
      return GPU.msSpace.isLive(object);
    }
    return super.isLive(object);
  }

  /**
   * This method is the core method during the trace of the object graph.
   * The role of this method is to:
   *
   * 1. Ensure the traced object is not collected.
   * 2. If this is the first visit to the object enqueue it to be scanned.
   * 3. Return the forwarded reference to the object.
   *
   * In this instance, we refer objects in the mark-sweep space to the
   * msSpace for tracing, and defer to the superclass for all others.
   *
   * @param object The object to be traced.
   * @return The new reference to the same object instance.
   */
  @Inline
  @Override
  public ObjectReference traceObject(ObjectReference object) {
    if (object.isNull()) return object;
    if (redirectGPUNodes) {
      Address refGraphNode = object.toAddress().loadAddress(VM.objectModel.GC_HEADER_OFFSET());
      if (!refGraphNode.isZero()) {
        gpuValues.push(refGraphNode);
      } else if (!Space.isInSpace(GPU.VM_SPACE, object)) {
        VM.assertions.fail("traceObject: Object is not in VM space but has no reference graph node");
      }
      return object;
    }
    VM.assertions.fail("not used");
    if (Space.isInSpace(GPU.MARK_SWEEP, object))
      return GPU.msSpace.traceObject(this, object);
    return super.traceObject(object);
  }

  /**
   * Process any remembered set entries.  This means enumerating the
   * mod buffer and for each entry, marking the object as unlogged
   * (we don't enqueue for scanning since we're doing a full heap GC).
   */
  protected void processRememberedSets() {
    if (modBuffer != null) {
      logMessage(5, "clearing modBuffer");
      while (!modBuffer.isEmpty()) {
        ObjectReference src = modBuffer.pop();
        HeaderByte.markAsUnlogged(src);
      }
    }
  }
}
