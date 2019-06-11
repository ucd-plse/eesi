#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include "llvm/IR/BasicBlock.h"
#include <iostream>
#include <string>

enum class Interval { BOT, LTZ, ZERO, GTZ, GEZ, LEZ, NTZ, TOP };

// Contains an interval and metadata
struct Constraint {
  Constraint() {}
  explicit Constraint(std::string fname) : fname(fname) {}
  Constraint(std::string fname, std::string value) : fname(fname) {
    if (value == "<0") {
      interval = Interval::LTZ;
    } else if (value == ">=0") {
      interval = Interval::GEZ;
    } else if (value == ">0") {
      interval = Interval::GTZ;
    } else if (value == "<=0") {
      interval = Interval::LEZ;
    } else if (value == "==0") {
      interval = Interval::ZERO;
    } else if (value == "!=0") {
      interval = Interval::NTZ;
    } else if (value == "top") {
      interval = Interval::TOP;
    } else if (value == "bottom") {
      interval = Interval::BOT;
    } else {
      std::cerr << "Constraint constructor called with unknown value " << value;
      exit(1);
    }
  }

  // The name of the function to be constrained
  std::string fname;

  // The lattice value
  Interval interval = Interval::BOT;

  // Debug info if we have it (not guaranteed) of icmp that generated this
  // constraint
  std::string file;
  unsigned line = 0;

  bool operator==(const Constraint &other) const {
    return (fname == other.fname && interval == other.interval &&
            file == other.file && line == other.line);
  }
  bool operator!=(const Constraint &other) const { return !(*this == other); }


  bool covers(const Interval other) const {
    if (interval == other) {
      return true;
    }
    if (interval == Interval::TOP) {
      return true;
    }
    if (other == Interval::TOP) {
      return false;
    }
    if (interval == Interval::BOT) {
      return false;
    }
    if (other == Interval::BOT) {
      return true;
    }
    if (interval == Interval::LTZ) {
      return false;
    }
    if (interval == Interval::ZERO) {
      return false;
    }
    if (interval == Interval::GTZ) {
      return false;
    }

    switch (interval) {
    case Interval::LEZ:
      switch (other) {
      case Interval::LTZ:
        return true;
      case Interval::ZERO:
        return true;
      case Interval::GTZ:
        return false;
      case Interval::NTZ:
        return false;
      case Interval::GEZ:
        return false;
      }
      break;
    case Interval::NTZ:
      switch (other) {
      case Interval::LTZ:
        return true;
      case Interval::ZERO:
        return false;
      case Interval::GTZ:
        return true;
      case Interval::LEZ:
        return false;
      case Interval::GEZ:
        return false;
      }
      break;
    case Interval::GEZ:
      switch (other) {
      case Interval::LTZ:
        return false;
      case Interval::ZERO:
        return true;
      case Interval::GTZ:
        return true;
      case Interval::LEZ:
        return false;
      case Interval::NTZ:
        return false;
      }
      break;
    default:
      abort();
    }
  } 

  Constraint join(const Constraint &other) {
    assert(fname == other.fname);

    Constraint ret(fname);
    ret.interval = interval;

    if (interval == other.interval) {
      return ret;
    }
    if (interval == Interval::TOP || other.interval == Interval::TOP) {
      ret.interval = Interval::TOP;
      return ret;
    }
    if (interval == Interval::BOT) {
      ret.interval = other.interval;
      return ret;
    }
    if (other.interval == Interval::BOT) {
      ret.interval = interval;
      return ret;
    }

    switch (interval) {
    case Interval::LTZ:
      switch (other.interval) {
      case Interval::ZERO:
        ret.interval = Interval::LEZ;
        break;
      case Interval::GTZ:
        ret.interval = Interval::NTZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::LEZ;
        break;
      case Interval::NTZ:
        ret.interval = Interval::NTZ;
        break;
      case Interval::GEZ:
        ret.interval = Interval::TOP;
        break;
      }
      break;
    case Interval::ZERO:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::LEZ;
        break;
      case Interval::GTZ:
        ret.interval = Interval::GEZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::LEZ;
        break;
      case Interval::NTZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::GEZ:
        ret.interval = Interval::GEZ;
        break;
      }
      break;
    case Interval::GTZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::NTZ;
        break;
      case Interval::ZERO:
        ret.interval = Interval::GEZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::NTZ:
        ret.interval = Interval::NTZ;
        break;
      case Interval::GEZ:
        ret.interval = Interval::GEZ;
        break;
      }
      break;
    case Interval::LEZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::LEZ;
        break;
      case Interval::ZERO:
        ret.interval = Interval::LEZ;
        break;
      case Interval::GTZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::NTZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::GEZ:
        ret.interval = Interval::TOP;
        break;
      }
      break;
    case Interval::NTZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::NTZ;
        break;
      case Interval::ZERO:
        ret.interval = Interval::TOP;
        break;
      case Interval::GTZ:
        ret.interval = Interval::NTZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::GEZ:
        ret.interval = Interval::TOP;
        break;
      }
      break;
    case Interval::GEZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::ZERO:
        ret.interval = Interval::GEZ;
        break;
      case Interval::GTZ:
        ret.interval = Interval::GEZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::TOP;
        break;
      case Interval::NTZ:
        ret.interval = Interval::TOP;
        break;
      }
      break;
    default:
      abort();
    }

    return ret;
  }

  Constraint meet(const Constraint &other) {
    assert(fname == other.fname);

    Constraint ret(fname);
    ret.interval = interval;

    if (interval == other.interval) {
      return ret;
    }

    if (interval == Interval::BOT || other.interval == Interval::BOT) {
      ret.interval = Interval::BOT;
      return ret;
    }
    if (interval == Interval::TOP) {
      ret.interval = other.interval;
      return ret;
    }
    if (other.interval == Interval::TOP) {
      ret.interval = interval;
      return ret;
    }

    switch (interval) {
    case Interval::LTZ:
      switch (other.interval) {
      case Interval::ZERO:
        ret.interval = Interval::BOT;
        break;
      case Interval::GTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::LEZ:
        ret.interval = Interval::LTZ;
        break;
      case Interval::NTZ:
        ret.interval = Interval::LTZ;
        break;
      case Interval::GEZ:
        ret.interval = Interval::BOT;
        break;
      }
      break;
    case Interval::ZERO:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::GTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::LEZ:
        ret.interval = Interval::ZERO;
        break;
      case Interval::NTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::GEZ:
        ret.interval = Interval::ZERO;
        break;
      }
      break;
    case Interval::GTZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::ZERO:
        ret.interval = Interval::BOT;
        break;
      case Interval::LEZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::NTZ:
        ret.interval = Interval::GTZ;
        break;
      case Interval::GEZ:
        ret.interval = Interval::GTZ;
        break;
      }
      break;
    case Interval::LEZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::LTZ;
        break;
      case Interval::ZERO:
        ret.interval = Interval::ZERO;
        break;
      case Interval::GTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::NTZ:
        ret.interval = Interval::LTZ;
        break;
      case Interval::GEZ:
        ret.interval = Interval::ZERO;
        break;
      }
      break;
    case Interval::NTZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::LTZ;
        break;
      case Interval::ZERO:
        ret.interval = Interval::BOT;
        break;
      case Interval::GTZ:
        ret.interval = Interval::GTZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::LTZ;
        break;
      case Interval::GEZ:
        ret.interval = Interval::GTZ;
        break;
      }
      break;
    case Interval::GEZ:
      switch (other.interval) {
      case Interval::LTZ:
        ret.interval = Interval::BOT;
        break;
      case Interval::ZERO:
        ret.interval = Interval::ZERO;
        break;
      case Interval::GTZ:
        ret.interval = Interval::GTZ;
        break;
      case Interval::LEZ:
        ret.interval = Interval::ZERO;
        break;
      case Interval::NTZ:
        ret.interval = Interval::GTZ;
        break;
      }
      break;
    default:
      abort();
    }
  }
};

inline std::ostream &operator<<(std::ostream &os, const Interval &interval) {
  if (interval == Interval::LTZ) {
    os << "<0";
  } else if (interval == Interval::GEZ) {
    os << ">=0";
  } else if (interval == Interval::GTZ) {
    os << ">0";
  } else if (interval == Interval::LEZ) {
    os << "<=0";
  } else if (interval == Interval::ZERO) {
    os << "==0";
  } else if (interval == Interval::NTZ) {
    os << "!=0";
  } else if (interval == Interval::TOP) {
    os << "top";
  } else if (interval == Interval::BOT) {
    os << "bottom";
  } else {
    os << "???";
  }

  return os;
}

inline std::ostream &operator<<(std::ostream &os,
                                const Constraint &constraint) {
  os << constraint.fname << " " << constraint.interval;
}

#endif
