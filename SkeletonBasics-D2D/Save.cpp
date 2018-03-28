#include "shcore.h"
#include "stdafx.h"
#include <strsafe.h>
#include "SkeletonBasics.h"
#include "resource.h"
#include "Wincodec.h"

ID2D1DeviceContext x;

ComPtr<IWICImagingFactory2> m_wicFactory;

DX::ThrowIfFailed(
	CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&m_wicFactory)
	)
);

void SaveAsImageFile::SaveBitmapToFile()
{
	// Prepare a file picker for customers to input image file name.
	Pickers::FileSavePicker^ savePicker = ref new Pickers::FileSavePicker();
	auto pngExtensions = ref new Platform::Collections::Vector<Platform::String^>();
	pngExtensions->Append(".png");
	savePicker->FileTypeChoices->Insert("PNG file", pngExtensions);
	auto jpgExtensions = ref new Platform::Collections::Vector<Platform::String^>();
	jpgExtensions->Append(".jpg");
	savePicker->FileTypeChoices->Insert("JPEG file", jpgExtensions);
	auto bmpExtensions = ref new Platform::Collections::Vector<Platform::String^>();
	bmpExtensions->Append(".bmp");
	savePicker->FileTypeChoices->Insert("BMP file", bmpExtensions);
	savePicker->DefaultFileExtension = ".png";
	savePicker->SuggestedFileName = "SaveScreen";
	savePicker->SuggestedStartLocation = Pickers::PickerLocationId::PicturesLibrary;

	task<StorageFile^> fileTask(savePicker->PickSaveFileAsync());
	fileTask.then([=](StorageFile^ file) {
		if (file != nullptr)
		{
			// User selects a file.
			m_imageFileName = file->Name;
			GUID wicFormat = GUID_ContainerFormatPng;
			if (file->FileType == ".bmp")
			{
				wicFormat = GUID_ContainerFormatBmp;
			}
			else if (file->FileType == ".jpg")
			{
				wicFormat = GUID_ContainerFormatJpeg;
			}

			// Retrieve a stream from the file.
			task<Streams::IRandomAccessStream^> createStreamTask(file->OpenAsync(FileAccessMode::ReadWrite));
			createStreamTask.then([=](Streams::IRandomAccessStream^ randomAccessStream) {
				// Convert the RandomAccessStream to an IStream.
				ComPtr<IStream> stream;
				DX::ThrowIfFailed(
					CreateStreamOverRandomAccessStream(randomAccessStream, IID_PPV_ARGS(&stream))
				);

				SaveBitmapToStream(m_d2dTargetBitmap, m_wicFactory, m_d2dContext, wicFormat, stream.Get());
			});
		}
	});
}

// Save render target bitmap to a stream using WIC.
void SaveAsImageFile::SaveBitmapToStream(
	_In_ ComPtr<ID2D1Bitmap1> d2dBitmap,
	_In_ ComPtr<IWICImagingFactory2> wicFactory2,
	_In_ ComPtr<ID2D1DeviceContext> d2dContext,
	_In_ REFGUID wicFormat,
	_In_ IStream* stream
)
{
	// Create and initialize WIC Bitmap Encoder.
	ComPtr<IWICBitmapEncoder> wicBitmapEncoder;
	DX::ThrowIfFailed(
		wicFactory2->CreateEncoder(
			wicFormat,
			nullptr,    // No preferred codec vendor.
			&wicBitmapEncoder
		)
	);

	DX::ThrowIfFailed(
		wicBitmapEncoder->Initialize(
			stream,
			WICBitmapEncoderNoCache
		)
	);

	// Create and initialize WIC Frame Encoder.
	ComPtr<IWICBitmapFrameEncode> wicFrameEncode;
	DX::ThrowIfFailed(
		wicBitmapEncoder->CreateNewFrame(
			&wicFrameEncode,
			nullptr     // No encoder options.
		)
	);

	DX::ThrowIfFailed(
		wicFrameEncode->Initialize(nullptr)
	);

	// Retrieve D2D Device.
	ComPtr<ID2D1Device> d2dDevice;
	d2dContext->GetDevice(&d2dDevice);

	// Create IWICImageEncoder.
	ComPtr<IWICImageEncoder> imageEncoder;
	DX::ThrowIfFailed(
		wicFactory2->CreateImageEncoder(
			d2dDevice.Get(),
			&imageEncoder
		)
	);

	DX::ThrowIfFailed(
		imageEncoder->WriteFrame(
			d2dBitmap.Get(),
			wicFrameEncode.Get(),
			nullptr     // Use default WICImageParameter options.
		)
	);

	DX::ThrowIfFailed(
		wicFrameEncode->Commit()
	);

	DX::ThrowIfFailed(
		wicBitmapEncoder->Commit()
	);

	// Flush all memory buffers to the next-level storage object.
	DX::ThrowIfFailed(
		stream->Commit(STGC_DEFAULT)
	);
}
