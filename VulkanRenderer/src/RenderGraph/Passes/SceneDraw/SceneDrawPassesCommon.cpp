#include "SceneDrawPassesCommon.h"

const RG::DrawAttachments& SceneDrawPassViewAttachments::Get(StringId viewName, StringId passName) const
{
    return m_Attachments.at(viewName).at(passName);
}

RG::DrawAttachments& SceneDrawPassViewAttachments::Get(StringId viewName, StringId passName)
{
    return const_cast<RG::DrawAttachments&>(
        const_cast<const SceneDrawPassViewAttachments&>(*this).Get(viewName, passName));
}

void SceneDrawPassViewAttachments::Add(StringId viewName, StringId passName, const RG::DrawAttachments& attachments)
{
    m_Attachments[viewName][passName] = attachments;
}
