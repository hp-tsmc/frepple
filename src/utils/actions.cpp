/***************************************************************************
 *                                                                         *
 * Copyright (C) 2007-2015 by frePPLe bv                                   *
 *                                                                         *
 * This library is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU Affero General Public License as published   *
 * by the Free Software Foundation; either version 3 of the License, or    *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
 * GNU Affero General Public License for more details.                     *
 *                                                                         *
 * You should have received a copy of the GNU Affero General Public        *
 * License along with this program.                                        *
 * If not, see <http://www.gnu.org/licenses/>.                             *
 *                                                                         *
 ***************************************************************************/

#define FREPPLE_CORE
#include "frepple/utils.h"

namespace frepple {
namespace utils {

//
// COMMAND LIST
//

void CommandList::add(Command* c) {
  // Validity check
  if (!c) throw LogicException("Adding nullptr command to a command list");

  // Set the owner of the command
  c->owner = this;

  // Maintenance of the linked list of child commands
  c->prev = lastCommand;
  if (lastCommand)
    // Let the last command in the chain point to this new extra command
    lastCommand->next = c;
  else
    // This is the first command in this command list
    firstCommand = c;
  lastCommand = c;
}

void CommandList::rollback() {
  // Undo all commands and delete them.
  // Note that undoing an operation that hasn't been executed yet or has been
  // undone already is expected to be harmless, so we don't need to worry
  // about that...
  for (auto i = lastCommand; i;) {
    Command* t = i;  // Temporarily store the pointer to be deleted
    i = i->prev;
    t->next = nullptr;
    delete t;  // The delete is expected to also revert the change!
  }

  // Reset the list
  firstCommand = nullptr;
  lastCommand = nullptr;
}

void CommandList::commit() {
  // Commit the commands
  for (auto i = firstCommand; i;) {
    Command* t = i;  // Temporarily store the pointer to be deleted
    i->commit();
    i = i->next;
    t->prev = nullptr;
    delete t;
  }

  // Reset the list
  firstCommand = nullptr;
  lastCommand = nullptr;
}

CommandList::~CommandList() {
  if (firstCommand) {
    logger << "Warning: Deleting a command list with commands that have"
           << " not been committed or rolled back" << endl;
    rollback();
  }
}

//
// COMMAND MANAGER
//

CommandManager::Bookmark* CommandManager::setBookmark() {
  Bookmark* n = new Bookmark(currentBookmark);
  lastBookmark->nextBookmark = n;
  n->prevBookmark = lastBookmark;
  lastBookmark = n;
  currentBookmark = n;
  return n;
}

void CommandManager::rollback(CommandManager::Bookmark* b) {
  if (!b) throw LogicException("Can't rollback nullptr bookmark");
  if (b == &firstBookmark)
    throw LogicException("Can't rollback default bookmark");

  // Remove all later child bookmarks
  Bookmark* i = lastBookmark;
  while (i && i != b) {
    if (i->isChildOf(b)) {
      // Remove from bookmark list
      if (i->prevBookmark) i->prevBookmark->nextBookmark = i->nextBookmark;
      if (i->nextBookmark)
        i->nextBookmark->prevBookmark = i->prevBookmark;
      else
        lastBookmark = i->prevBookmark;
      i->rollback();
      if (currentBookmark == i) currentBookmark = b;
      Bookmark* tmp = i;
      i = i->prevBookmark;
      delete tmp;
    } else
      // Bookmark has a different parent
      i = i->prevBookmark;
  }
  if (!i) throw LogicException("Can't find bookmark to rollback");
  b->rollback();
}

void CommandManager::commit() {
  if (firstBookmark.active) firstBookmark.commit();
  for (auto i = firstBookmark.nextBookmark; i;) {
    if (i->active) i->commit();
    Bookmark* tmp = i;
    i = i->nextBookmark;
    delete tmp;
  }
  firstBookmark.nextBookmark = nullptr;
  currentBookmark = &firstBookmark;
  lastBookmark = &firstBookmark;
}

void CommandManager::rollback() {
  for (auto i = lastBookmark; i != &firstBookmark;) {
    i->rollback();
    Bookmark* tmp = i;
    i = i->prevBookmark;
    delete tmp;
  }
  firstBookmark.rollback();
  firstBookmark.nextBookmark = nullptr;
  currentBookmark = &firstBookmark;
  lastBookmark = &firstBookmark;
}

bool CommandManager::empty() const {
  if (firstBookmark.active && !firstBookmark.empty()) return false;
  for (auto bkmrk = firstBookmark.nextBookmark; bkmrk;
       bkmrk = bkmrk->nextBookmark) {
    if (!bkmrk->empty()) return false;
  }
  return true;
}

//
// COMMAND SETPROPERTY
//

CommandSetProperty::CommandSetProperty(Object* o, const string& nm,
                                       const DataValue& value, short tp)
    : obj(o), name(nm), type(tp) {
  if (!o || nm.empty()) return;

  // Store old value
  old_exists = o->hasProperty(name);
  if (old_exists) {
    switch (type) {
      case 1:  // Boolean
        old_bool = obj->getBoolProperty(name);
        break;
      case 2:  // Date
        old_date = obj->getDateProperty(name);
        break;
      case 3:  // Double
        old_double = obj->getDoubleProperty(name);
        break;
      case 4:  // String
        old_string = obj->getStringProperty(name);
        break;
      default:
        break;
    }
  }
}

void CommandSetProperty::rollback() {
  if (!obj || name.empty()) {
    if (old_exists && obj) {
      switch (type) {
        case 1:  // Boolean
        {
          bool tmp_bool = obj->getBoolProperty(name);
          obj->setBoolProperty(name, old_bool);
          old_bool = tmp_bool;
        } break;
        case 2:  // Date
        {
          Date tmp_date = obj->getDateProperty(name);
          obj->setDateProperty(name, old_date);
          old_date = tmp_date;
        } break;
        case 3:  // Double
        {
          double tmp_double = obj->getDoubleProperty(name);
          obj->setDoubleProperty(name, old_double);
          old_double = tmp_double;
        } break;
        case 4:  // String
        {
          string tmp_string = obj->getStringProperty(name);
          obj->setStringProperty(name, old_string);
          old_string = tmp_string;
        } break;
        default:
          break;
      }
    } else if (obj) {
      switch (type) {
        case 1:  // Boolean
          old_bool = obj->getBoolProperty(name);
          break;
        case 2:  // Date
          old_date = obj->getDateProperty(name);
          break;
        case 3:  // Double
          old_double = obj->getDoubleProperty(name);
          break;
        case 4:  // String
          old_string = obj->getStringProperty(name);
          break;
        default:
          break;
      }
      obj->deleteProperty(name);
    }
  }
  obj = nullptr;
  name = "";
}

//
// THREAD GROUP
//

void ThreadGroup::execute() {
  // Determine the number of threads
  auto numthreads = callables.size();
  if (numthreads > static_cast<size_t>(maxParallel)) numthreads = maxParallel;

  if (numthreads <= 1)
    // Sequential execution
    wrapper(this);
  else {
    // Parallel execution in worker threads
    stack<thread> threads;

    // Launch all threads
    while (numthreads > 0) {
      threads.push(thread(wrapper, this));
      --numthreads;
    }

    // Wait for all threads to finish
    while (!threads.empty()) {
      threads.top().join();
      threads.pop();
    }
  }
}

ThreadGroup::callableWithArgument ThreadGroup::selectNextCallable() {
  lock_guard<mutex> l(lock);
  if (callables.empty())
    // No more functions
    return callableWithArgument(static_cast<callable>(nullptr),
                                static_cast<void*>(nullptr));
  callableWithArgument c = callables.top();
  callables.pop();
  return c;
}

void ThreadGroup::wrapper(ThreadGroup* grp) {
  while (true) {
    auto job = grp->selectNextCallable();
    if (!job.first) return;
    try {
      job.first(job.second);
    } catch (...) {
      // Error message
      logger << "Error: Caught an exception while executing command:" << endl;
      try {
        throw;
      } catch (const exception& e) {
        logger << "  " << e.what() << endl;
      } catch (...) {
        logger << "  Unknown type" << endl;
      }
    }
  };
}

//
// LOADMODULE FUNCTION
//

PyObject* loadModule(PyObject* self, PyObject* args, PyObject* kwds) {
  // Create the command
  char* data = nullptr;
  int ok = PyArg_ParseTuple(args, "s:loadmodule", &data);
  if (!ok || !data) return nullptr;

  // Free Python interpreter for other threads.
  // This is important since the module may also need access to Python
  // during its initialization...
  Py_BEGIN_ALLOW_THREADS;
  try {
    // Load the library
    Environment::loadModule(data);
  } catch (...) {
    Py_BLOCK_THREADS;
    PythonType::evalException();
    return nullptr;
  }
  // Reclaim Python interpreter
  Py_END_ALLOW_THREADS;
  return Py_BuildValue("");
}

void Environment::printModules() {
  bool first = true;
  for (auto i = moduleRegistry.begin(); i != moduleRegistry.end(); ++i) {
    if (first) {
      logger << "Loaded modules:" << endl;
      first = false;
    }
    logger << "   " << *i << endl;
  }
  if (!first) logger << endl;
}

}  // namespace utils
}  // namespace frepple
