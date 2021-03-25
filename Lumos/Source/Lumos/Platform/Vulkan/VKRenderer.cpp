#include "Precompiled.h"
#include "VKRenderer.h"
#include "VKDevice.h"
#include "VKShader.h"
#include "VKDescriptorSet.h"
#include "VKTools.h"
#include "VKPipeline.h"
#include "Core/Engine.h"
 
namespace Lumos
{
	namespace Graphics
	{
		void VKRenderer::InitInternal()
		{
			LUMOS_PROFILE_FUNCTION();

			m_RendererTitle = "Vulkan";

			CreateSemaphores();
		}

		VKRenderer::~VKRenderer()
		{
		}

		void VKRenderer::PresentInternal(CommandBuffer* cmdBuffer)
		{
			LUMOS_PROFILE_FUNCTION();
		}
    
        void VKRenderer::ClearRenderTarget(Graphics::Texture* texture, Graphics::CommandBuffer* cmdBuffer)
        {
            VkImageSubresourceRange subresourceRange = {}; //TODO: Get from texture
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.layerCount = 1;
            subresourceRange.levelCount = 1;
            
            //TODO: Pass clear Value
            //TODO: Handle Depth/Stencil
            
            VkClearColorValue clearColourValue = VkClearColorValue({{0.0f, 0.0f, 0.0f, 0.0f}});
            vkCmdClearColorImage(((VKCommandBuffer*)cmdBuffer)->GetCommandBuffer(), static_cast<VKTexture2D*>(texture)->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &clearColourValue, 1, &subresourceRange);
        }

		void VKRenderer::ClearSwapchainImage() const
		{
			LUMOS_PROFILE_FUNCTION();
            
            auto m_Swapchain = VKContext::Get()->GetSwapchain();
			for(int i = 0; i < m_Swapchain->GetSwapchainBufferCount(); i++)
			{
				auto cmd = VKTools::BeginSingleTimeCommands();

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.layerCount = 1;
				subresourceRange.levelCount = 1;

				VkClearColorValue clearColourValue = VkClearColorValue({{0.0f, 0.0f, 0.0f, 0.0f}});

				vkCmdClearColorImage(cmd, static_cast<VKTexture2D*>(m_Swapchain->GetImage(i))->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &clearColourValue, 1, &subresourceRange);

				VKTools::EndSingleTimeCommands(cmd);
			}
		}

		void VKRenderer::OnResize(uint32_t width, uint32_t height)
		{
			LUMOS_PROFILE_FUNCTION();
			if(width == 0 || height == 0)
				return;

			m_Width = width;
			m_Height = height;
            
            LUMOS_LOG_INFO("{0},{1}", width, height);
			
			VkSurfaceCapabilitiesKHR capabilities;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VKDevice::Get().GetGPU(), VKContext::Get()->GetSwapchain()->GetSurface(), &capabilities);
			
			m_Width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, m_Width));
			m_Height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, m_Height));
			
			
			VKContext::Get()->OnResize(m_Width, m_Height);
        }

		void VKRenderer::CreateSemaphores()
		{
		}
    
        Swapchain* VKRenderer::GetSwapchainInternal() const
        {
            return VKContext::Get()->GetSwapchain().get();
        }

		void VKRenderer::Begin()
		{
			LUMOS_PROFILE_FUNCTION();
            AcquireNextImage();

			GetSwapchainInternal()->GetCurrentCommandBuffer()->BeginRecording();
		}
    
        void VKRenderer::AcquireNextImage()
        {
            m_Context = VKContext::Get();
            auto swapchain = m_Context->GetSwapchain();

            auto result = swapchain->AcquireNextImage(nullptr);
            if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            {
               LUMOS_LOG_INFO("Acquire Image result : {0}", result == VK_ERROR_OUT_OF_DATE_KHR ? "Out of Date" : "SubOptimal");
                
                if(result == VK_ERROR_OUT_OF_DATE_KHR)
                    OnResize(m_Width, m_Height);
                return;
            }
            else if(result != VK_SUCCESS)
            {
                LUMOS_LOG_CRITICAL("[VULKAN] Failed to acquire swap chain image!");
            }
        }
    
        void VKRenderer::PresentInternal()
        {
            LUMOS_PROFILE_FUNCTION();
            GetSwapchainInternal()->GetCurrentCommandBuffer()->EndRecording();
                        
//            static_cast<VKCommandBuffer*>(GetSwapchainInternal()->GetCurrentCommandBuffer())->ExecuteInternal(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//                static_cast<VKSwapchain*>(m_Context->GetSwapchain().get())->GetImageAcquiredSemaphore(),
//                nullptr,
//                true);
                    
            VKContext::Get()->GetSwapchain()->Present(nullptr);
        }

		const std::string& VKRenderer::GetTitleInternal() const
		{
			return m_RendererTitle;
		}

		void VKRenderer::BindDescriptorSetsInternal(Graphics::Pipeline* pipeline, Graphics::CommandBuffer* cmdBuffer, uint32_t dynamicOffset, std::vector<Graphics::DescriptorSet*>& descriptorSets)
		{
			LUMOS_PROFILE_FUNCTION();
			uint32_t numDynamicDescriptorSets = 0;
			uint32_t numDesciptorSets = 0;

			for(auto descriptorSet : descriptorSets)
			{
                if(descriptorSet)
                {
                    auto vkDesSet = static_cast<Graphics::VKDescriptorSet*>(descriptorSet);
                    if(vkDesSet->GetIsDynamic())
                        numDynamicDescriptorSets++;

                    m_DescriptorSetPool[numDesciptorSets] = vkDesSet->GetDescriptorSet();

                    numDesciptorSets++;
                }
			}

			vkCmdBindDescriptorSets(static_cast<Graphics::VKCommandBuffer*>(cmdBuffer)->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<Graphics::VKPipeline*>(pipeline)->GetPipelineLayout(), 0, numDesciptorSets, m_DescriptorSetPool, numDynamicDescriptorSets, &dynamicOffset);
		}

		void VKRenderer::DrawIndexedInternal(CommandBuffer* commandBuffer, DrawType type, uint32_t count, uint32_t start) const
		{
			LUMOS_PROFILE_FUNCTION();
			Engine::Get().Statistics().NumDrawCalls++;
			vkCmdDrawIndexed(static_cast<VKCommandBuffer*>(commandBuffer)->GetCommandBuffer(), count, 1, 0, 0, 0);
		}

		void VKRenderer::DrawInternal(CommandBuffer* commandBuffer, DrawType type, uint32_t count, DataType datayType, void* indices) const
		{
			LUMOS_PROFILE_FUNCTION();
            Engine::Get().Statistics().NumDrawCalls++;
			vkCmdDraw(static_cast<VKCommandBuffer*>(commandBuffer)->GetCommandBuffer(), count, 1, 0, 0);
		}

		void VKRenderer::MakeDefault()
		{
			CreateFunc = CreateFuncVulkan;
		}

		Renderer* VKRenderer::CreateFuncVulkan(uint32_t width, uint32_t height)
		{
			return new VKRenderer(width, height);
		}
	}
}