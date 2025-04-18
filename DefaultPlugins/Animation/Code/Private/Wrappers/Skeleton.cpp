#include "SkeletonInstance.h"

#include "FileStream.h"
#include "DataStream.h"

#include <Common/IO/FileView.h>
#include <Common/Memory/Containers/ByteView.h>

#include "3rdparty/ozz/base/io/archive.h"
#include "3rdparty/ozz/animation/runtime/local_to_model_job.h"
#include "3rdparty/ozz/base/maths/soa_transform.h"

namespace ngine::Animation
{
	bool Skeleton::Load(const ConstByteView data)
	{
		DataStream stream(ByteView{const_cast<ByteType*>(data.GetData()), data.GetDataSize()});
		ozz::io::IArchive archive(&stream);
		if (!archive.TestTag<ozz::animation::Skeleton>())
		{
			return false;
		}

		// Once the tag is validated, reading cannot fail.
		archive >> static_cast<BaseType&>(*this);

		return true;
	}

	bool Skeleton::Load(const IO::FileView file)
	{
		FileStream stream(file);
		ozz::io::IArchive archive(&stream);
		if (!archive.TestTag<ozz::animation::Skeleton>())
		{
			return false;
		}

		// Once the tag is validated, reading cannot fail.
		archive >> static_cast<BaseType&>(*this);

		return true;
	}

	void Skeleton::Save(const IO::FileView file) const
	{
		Assert(file.IsValid());
		FileStream stream{file};
		ozz::io::OArchive archive(&stream);
		archive << static_cast<const BaseType&>(*this);
	}

	void SkeletonInstance::ProcessLocalToModelSpace(
		const Skeleton& skeleton,
		const ArrayView<const ozz::math::SoaTransform, uint16> transforms,
		const ArrayView<Math::Matrix4x4f, uint16> modelSpaceMatricesOut
	)
	{
		// Converts from local space to model space matrices.
		ozz::animation::LocalToModelJob localToModelSpaceJob;
		localToModelSpaceJob.skeleton = &skeleton.GetOzzType();
		localToModelSpaceJob.input = ozz::span<const ozz::math::SoaTransform>{transforms.GetData(), transforms.GetSize()};
		localToModelSpaceJob.output = ozz::span<ozz::math::Float4x4>{
			&reinterpret_cast<ozz::math::Float4x4&>(*modelSpaceMatricesOut.GetData()),
			modelSpaceMatricesOut.GetSize()
		};
		localToModelSpaceJob.Run();
	}
}
