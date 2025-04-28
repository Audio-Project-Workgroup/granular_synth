enum UIFringeFlags
{
  UIFringeFlag_right  = 1 << 0,
  UIFringeFlag_top    = 1 << 1,
  UIFringeFlag_left   = 1 << 2,
  UIFringeFlag_bottom = 1 << 3,
};

struct UIPanel
{
  UIPanel *first;
  UIPanel *last;
  UIPanel *next;
  UIPanel *prev;  
  UIPanel *parent;

  UIAxis splitAxis;
  r32 sizePercentOfParent;
  
  String8 name;
  v4 color;
  LoadedBitmap *texture;

  u32 fringeFlags;
  Rect2 region;

  UILayout layout;
};

inline UIPanel *
makeUIPanel(UIPanel *currentParent, Arena *permanentArena, UIAxis splitAxis, r32 sizePercentOfParent, String8 name,
	    LoadedBitmap *texture = 0, v4 color = V4(0, 0, 0, 1))
{
  UIPanel *newPanel = arenaPushStruct(permanentArena, UIPanel, arenaFlagsZeroNoAlign());
  newPanel->splitAxis = splitAxis;
  newPanel->sizePercentOfParent = sizePercentOfParent;
  newPanel->parent = currentParent;
  newPanel->name = arenaPushString(permanentArena, name);
  newPanel->texture = texture;
  newPanel->color = color;
  DLL_PUSH_BACK(currentParent->first, currentParent->last, newPanel);

  return(newPanel);
}

struct UIPanelIterator
{
  UIPanel *next;
  s32 pushCount;
  s32 popCount;
};

inline UIPanelIterator
uiPanelIteratorDepthFirstPreorder(UIPanel *panel)
{
  UIPanelIterator iter = {};
  if(panel->first)
    {
      iter.next = panel->first;
      iter.pushCount = 1;
    }
  else
    {
      for(UIPanel *p = panel; p; p = p->parent)
	{
	  if(p->next)
	    {
	      iter.next = p->next;
	      break;
	    }

	  iter.popCount += 1;
	}
    }

  return(iter);
}

struct RectNode
{
  RectNode *next;
  Rect2 rect;
};

inline Rect2
uiComputeChildPanelRect(UIPanel *child, Rect2 parentRect)
{
  Rect2 result = parentRect;  
  
  UIPanel *parent = child->parent;
  if(parent)
    {      
      UIAxis splitAxis = parent->splitAxis;
      v2 parentRectDim = getDim(parentRect);

      result.max.E[splitAxis] = result.min.E[splitAxis];
      for(UIPanel *p = parent->first; p && p != child; p = p->next)
	{
	  result.min.E[splitAxis] += p->sizePercentOfParent*parentRectDim.E[splitAxis];
	  result.max.E[splitAxis] = result.min.E[splitAxis];
	}
      result.max.E[splitAxis] += child->sizePercentOfParent*parentRectDim.E[splitAxis];
    }

  return(result);
}

struct WalkNode
{
  WalkNode *next;
  UIPanel *parent;
  UIPanel *child;
};

inline Rect2
uiComputeRectFromPanel(UIPanel *panel, Rect2 rootRect, Arena *scratch)
{
  WalkNode *firstAncestorNode = 0;
  for(UIPanel *p = panel; p && p->parent; p = p->parent)
    {
      WalkNode *node = arenaPushStruct(scratch, WalkNode);
      STACK_PUSH(firstAncestorNode, node);
      node->parent = p->parent;
      node->child = p;
    }

  Rect2 result = rootRect;
  for(WalkNode *node = firstAncestorNode; node; node = node->next)
    {
      result = uiComputeChildPanelRect(node->child, result);
    }

  return(result);
}
