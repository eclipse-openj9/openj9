
#include "XMLMethodTreeWriter.hpp"

#include <memory>
#include <set>
#include <atomic>
#include "env/FilePointerDecl.hpp"
#include "compile/Compilation.hpp"
#include "compile/Compilation_inlines.hpp"
#include "compile/OMRCompilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/OMRILOps.hpp"
#include "control/OMROptions.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Structure.hpp"

void replace(std::string & string, std::string target, std::string replacement) {
  size_t index = 0;
  auto n = target.size();
  while (true) {
      /* Locate the substring to replace. */
      index = string.find(target, index);
      if (index == std::string::npos) break;

      /* Make the replacement. */
      string.replace(index, n, replacement);

      /* Advance index forward so the next iteration doesn't pick it up as well. */
      index += n;
  }
}

void sanitizeSlashes(std::string & string) {
  std::replace(string.begin(), string.end(), '/', '.');
  std::replace(string.begin(), string.end(), ';', ' ');
}

void sanitizeXML(std::string & string) {
  replace(string, "<", "&lt;");
  replace(string, ">", "&gt;");
}

void sanitize(std::string & string) {
  sanitizeSlashes(string);
  sanitizeXML(string);
}

void openHeader(XMLMethodTreeWriter & writer, std::string str) {
  sanitize(str);
  writer.getSink()->write(string_format("<%s>\n", str.c_str()));
}

void closeHeader(XMLMethodTreeWriter & writer, std::string str) {
  sanitize(str);
  writer.getSink()->write(string_format("</%s>\n", str.c_str()));
}

void writeInlineElement(XMLMethodTreeWriter & writer, std::string str) {
  sanitize(str);
  writer.getSink()->write(string_format("<%s/>\n", str.c_str()));
}

void writeString(XMLMethodTreeWriter & writer, std::string str) {
  auto s = str;
  sanitizeXML(s);
  writer.getSink()->write(s.c_str());
}

void writeStringProperty(XMLMethodTreeWriter & writer, StringProperty & property) {
  openHeader(writer, string_format("p name='%s'", property.key.c_str()));
  writeString(writer, property.value);
  closeHeader(writer, "p");
}

void writeIntegerProperty(XMLMethodTreeWriter & writer, IntegerProperty & property) {
  openHeader(writer, string_format("p name='%s'", property.name.c_str()));
  writeString(writer, string_format("%d", property.value));
  closeHeader(writer, "p");
}

std::string getCategory(TR::Node * node) {
  auto opcode = node->getOpCode();

  if (opcode.isTreeTop())
    return "control";

  return "data";
}

void writeProperties(XMLMethodTreeWriter & writer, Properties & properties) {
  openHeader(writer, "properties");
  for (auto s : properties.strings) {
    writeStringProperty(writer, s);
  }
  for (auto i: properties.integers) {
    writeIntegerProperty(writer, i);
  }
  closeHeader(writer,"properties");
}

std::string getNodeName(TR::Compilation * compilation, TR::Node * node) {
  std::string name = node->getOpCode().getName();

  switch (node->getOpCodeValue()) {
    case TR::iconst:
      name = string_format("%s %d", name.c_str(), node->getInt());
      break;
    case TR::lconst:
      name = string_format("%s %ld", name.c_str(), node->getLongInt());
      break;
    case TR::bconst:
      name = string_format("%s %d", name.c_str(), node->getByte());
      break;
    case TR::sconst:
      name = string_format("%s %d", name.c_str(), node->getShortInt());
      break;
    case TR::aload:
    case TR::bload:
    case TR::sload:
    case TR::iload:
    case TR::lload:
    case TR::fload:
    case TR::dload:
    case TR::aloadi:
    case TR::bloadi:
    case TR::sloadi:
    case TR::iloadi:
    case TR::lloadi:
    case TR::floadi:
    case TR::dloadi:
    case TR::loadaddr:
    case TR::bstore:
    case TR::sstore:
    case TR::istore:
    case TR::fstore:
    case TR::dstore:
    case TR::icalli:
    case TR::icall:
    case TR::lcalli:
    case TR::lcall:
    case TR::fcalli:
    case TR::fcall:
    case TR::dcalli:
    case TR::dcall:
    case TR::acalli:
    case TR::acall:
    case TR::calli:
    case TR::call:
      name = string_format("%s %s", name.c_str(), node->getSymbolReference()->getName(compilation->getDebug()));
      break;
    case TR::aRegLoad:
    case TR::bRegLoad:
    case TR::sRegLoad:
    case TR::iRegLoad:
    case TR::fRegLoad:
    case TR::lRegLoad:
    case TR::dRegLoad:
      name = string_format("%s %s", node->getRegister()->getRegisterName(compilation));
      break;
    default:
      return name;
  }

  return name;
}

bool XMLMethodTreeWriter::initialize(TR::Compilation * compilation, TR::ResolvedMethodSymbol * symbol) {
  auto id = getNextAvailableCompilationId();
  auto method = symbol->getResolvedMethod();

  std::string signature = compilation->getDebug()->signature(symbol);
  std::replace(signature.begin(), signature.end(), '/', '.');
  auto hotness = compilation->getHotnessName(compilation->getMethodHotness());


  sink = new FileSink(
    string_format("TestarossaCompilation-%d[%s][%s].xml", id, signature.c_str(), hotness)
  );

  openHeader(*this, "graphDocument");
  openHeader(*this, "group");

  auto properties = Properties{}
    .add(StringProperty{"name", signature})
    .add(IntegerProperty{"compilationId", id});

  writeProperties(*this, properties);

  return true;
}

XMLMethodTreeWriter::XMLMethodTreeWriter(int32_t id, TR_ResolvedMethod * method, TR::Options & options): sink(nullptr) {}

XMLMethodTreeWriter::~XMLMethodTreeWriter() {}

Properties getBlockStartProperties(Properties & properties, TR::Compilation * compilation, TR:: Node * node) {
  TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Must be BBStart");
  TR::Block * block = node->getBlock();

  if (block->getFrequency() >= 0)
    properties.add(IntegerProperty{"frequency", block->getFrequency()});
  if (block->isExtensionOfPreviousBlock())
    properties.add(StringProperty{"isExtension", "true"});

  if (block->isSuperCold())
    properties.add(StringProperty{"isSuperCold", "true"});
  else if (block->isCold())
    properties.add(StringProperty{"isCold", "true"});

  if(block->isLoopInvariantBlock())
    properties.add(StringProperty{"isLoopInvariant", "true"});

  TR_BlockStructure *blockStructure = block->getStructureOf();
  if (compilation->getFlowGraph()->getStructure() && blockStructure) {
    TR_Structure * parent = blockStructure->getParent();
    while (parent) {
      TR_RegionStructure *region = parent->asRegion();
      if (region->isNaturalLoop() || region->containsInternalCycles()) {
        properties.add(IntegerProperty{"loop", region->getNumber()});
        break;
      }
      parent = parent->getParent();
    }
    TR_BlockStructure *dupBlock = blockStructure->getDuplicatedBlock();
    if (dupBlock)
      properties.add(IntegerProperty{"duplicate", dupBlock->getNumber()});
  }
  return properties;
}

Properties getOtherNodeProperties(Properties & properties, TR::Compilation * compilation, TR::Node * node) {
  switch (node->getOpCodeValue()) {
    case TR::BBStart:
      return getBlockStartProperties(properties, compilation, node);
    default:
      return properties;
  }
}

Properties getNodeProperties(TR::Compilation * compilation, TR::Node * node) {
  auto common = Properties{}
    .add(StringProperty{"name",     getNodeName(compilation, node)})
    .add(StringProperty{"category", getCategory(node)})
    .add(IntegerProperty{"idx",      (int32_t) node->getGlobalIndex()});

  return getOtherNodeProperties(common, compilation, node);
}

void writeNode(XMLMethodTreeWriter & writer, TR::Compilation * compilation, TR::Node * node) {
  openHeader(writer, string_format("node id='%d'\n", node->getGlobalIndex()));

  auto properties = getNodeProperties(compilation, node);
  writeProperties(writer, properties);
  closeHeader(writer, "node");
}

void writeTreeTop(XMLMethodTreeWriter & writer, TR::Compilation * compilation, TR::TreeTop * tt) {
  auto node = tt->getNode();
  writeNode(writer, compilation, node);
}

void writeEdge(XMLMethodTreeWriter & writer, int32_t from, int32_t to, std::string && type, int index) {
  writeInlineElement(
    writer,
    string_format("edge from='%d' to='%d' type='%s' index='%d'", from, to, type.c_str(), index)
  );
}

void writeEdges(XMLMethodTreeWriter & writer, std::vector<TR::TreeTop *> treetops, std::vector<TR::Node *> nodes) {
  openHeader(writer, "edges");
  for (auto tt : treetops) {
    auto node = tt->getNode();
    auto globalIndex = node->getGlobalIndex();
    auto next = tt->getNextTreeTop();

    if (next)
      writeEdge(writer, node->getGlobalIndex(), next->getNode()->getGlobalIndex(), "next", 0);

    if (node->getOpCode().isBranch())
      writeEdge(writer, globalIndex, node->getBranchDestination()->getNode()->getGlobalIndex(), "branchTrue", 1);

    for (auto i = 0; i < node->getNumChildren(); i++) {
      auto child = node->getChild(i);
      writeEdge(writer, child->getGlobalIndex(), node->getGlobalIndex(), "child", i);
    }
  }

  for (auto node : nodes) {
    for (auto i = 0; i < node->getNumChildren(); i++) {
      auto child = node->getChild(i);
      writeEdge(writer, child->getGlobalIndex(), node->getGlobalIndex(), "child", i);
    }
  }
  closeHeader(writer, "edges");
}

void writeBlocks(XMLMethodTreeWriter & writer, TR::Compilation * compilation, TR::CFG * cfg) {
  openHeader(writer, "controlFlow");

  std::set<int32_t> nodeSet{};
  for (TR::AllBlockIterator iter(cfg, compilation); iter.currentBlock(); ++iter) {
    auto block = iter.currentBlock();
    auto blockNumber = block->getNumber();
    openHeader(writer, string_format("block name='%d'", blockNumber));

    openHeader(writer, "nodes");

    auto entry = block->getEntry();
    auto exit = block->getExit();
    TR_ASSERT(exit->getNode()->getOpCodeValue() == TR::BBEnd, "The exit treetop must be a BBEnd");

    std::vector<TR::TreeTop *> treetops{};

    for (TR::TreeTopIterator it(entry, compilation); it != exit; ++it) {
      auto tt = it.currentTree();
      treetops.push_back(tt);
    }
    treetops.push_back(exit);

    for (auto tt: treetops) {
      for (TR::PreorderNodeIterator it(tt, compilation); it.currentTree() == tt; ++it) {
        auto node = it.currentNode();
        auto index = node->getGlobalIndex();
        if (nodeSet.count(index) > 0)
          continue;

        nodeSet.emplace(index);
        writeInlineElement(writer, string_format("node id='%d'", index));
      }
    }
    closeHeader(writer, "nodes");

    openHeader(writer, "successors");
    TR::CFGEdgeList & successors = block->getSuccessors();
    for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge) {
      auto number = (*succEdge)->getTo()->getNumber();
      if (number == 1)
        continue;
      writeInlineElement(writer, string_format("successor name='%d'", number));
    }

    auto exceptionSuccessors = block->getExceptionSuccessors();
    for (auto succEdge = exceptionSuccessors.begin(); succEdge != exceptionSuccessors.end(); ++succEdge) {
      auto number = (*succEdge)->getTo()->getNumber();
      if (number == 1)
        continue;

      writeInlineElement(writer, string_format("successor name='%d'", number));
    }

    closeHeader(writer, "successors");
    closeHeader(writer, "block");
  }
  closeHeader(writer, "controlFlow");
}

void XMLMethodTreeWriter::writeGraph(std::string & string, TR::Compilation * compilation, TR::ResolvedMethodSymbol * symbol) {
  if (!initialized)
    initialized = initialize(compilation, symbol);

  openHeader(*this,string_format("graph name = '%s'", string.c_str()));

  auto properties = Properties{};
  writeProperties(*this, properties);

  int32_t nodes_count = 0;
  auto firstTreeTop = symbol->getFirstTreeTop();

  std::vector<TR::Node *> nodes{};
  std::vector<TR::TreeTop *> treetops{};
  for (TR::TreeTopIterator ttCursor (symbol->getFirstTreeTop(), compilation); ttCursor != NULL; ++ttCursor) {
    TR::TreeTop * tt = ttCursor.currentTree();
    nodes.push_back(tt->getNode());
    treetops.push_back(tt);
  }


  for (TR::PreorderNodeIterator iter (symbol->getFirstTreeTop(), compilation); iter != NULL; ++iter) {
    auto node = iter.currentNode();
    nodes.push_back(node);
  }

  openHeader(*this, "nodes");
  for (auto tt: treetops) {
    writeTreeTop(*this, compilation, tt);
  }

  for (auto node: nodes) {
    writeNode(*this, compilation, node);
  }
  closeHeader(*this, "nodes");

  writeEdges(*this, treetops, nodes);

  writeBlocks(*this, compilation, symbol->getFlowGraph());
  closeHeader(*this, "graph");
}


