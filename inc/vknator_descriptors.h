﻿#pragma once

#include <vector>
#include <vknator_types.h>
#include <deque>
#include <span>

//> descriptor_layout
struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages);
};
//< descriptor_layout
//
//> writer
struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
    void write_buffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type);

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};

//> descriptor_allocator_grow
struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};
//< descriptor_allocator_grow