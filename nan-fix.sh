#!/bin/bash

files=$@
replacements=(
	"NanAsyncWorker/Nan::AsyncWorker"
	"NanAsyncQueueWorker/Nan::AsyncQueueWorker"
	"NanCallback/Nan::Callback"
	"NanNewBufferHandle\\(([^;]+);/Nan::NewBuffer(\\1.ToLocalChecked();"
	"(NanNew(<(v8::)?String>)?\\(\"[^\"]*\"\\))/\\1.ToLocalChecked()"
	"(NanNew<(v8::)?String>\\([^\"][^\;]*);/\\1.ToLocalChecked();"
	"NanNew/Nan::New"
	"NODE_SET_PROTOTYPE_METHOD/Nan::SetPrototypeMethod"
	"NODE_SET_METHOD/Nan::SetMethod"
	"_NAN_METHOD_ARGS_TYPE/Nan::NAN_METHOD_ARGS_TYPE"
	"(\\W)?args(\\W)/\\1info\\2"
	"(^|\\s)(v8::)?Persistent/\\1Nan::Persistent"
	"NanPersistent/Nan::Persistent"
	"NanAssignPersistent(<\w+>)?\\(([^,]+),\\s*([^)]+)\\)/\\2.Reset(\\3)"
	"NanDisposePersistent\\(([^\\)]+)\\)/\\1.Reset()"
	"NanReturnValue/info.GetReturnValue().Set"
	"NanReturnNull\\(\\)/info.GetReturnValue().Set(Nan::Null())"
	"NanScope\\(\\)/Nan::HandleScope\ scope"
	"NanScope/Nan::HandleScope"
	"NanEscapableScope\\(\\)/Nan::EscapableHandleScope scope"
	"NanEscapableScope/Nan::EscapableHandleScope"
	"NanEscapeScope/scope.Escape"
	"NanReturnUndefined\\(\\);/return;"
	"NanUtf8String/Nan::Utf8String"
	"NanObjectWrapHandle\\(([^\\)]+)\\)/\\1->handle()"
	"(node::)?ObjectWrap/Nan::ObjectWrap"
	"NanMakeCallback/Nan::MakeCallback"
	"NanNull/Nan::Null"
	"NanUndefined/Nan::Undefined"
	"NanFalse/Nan::False"
	"NanTrue/Nan::True"
	"NanThrow(\w+)?Error/Nan::Throw\\1Error"
	"NanError/Nan::Error"
	"NanGetCurrentContext/Nan::GetCurrentContext"
	"([a-zA-Z0-9_]+)->SetAccessor\\(/Nan::SetAccessor(\\1, "
	"NanAdjustExternalMemory/Nan::AdjustExternalMemory"
	"NanSetTemplate/Nan::SetTemplate"
	"NanHasInstance\\(([^,]+),\\s*([^)]+)\\)/Nan::New(\\1)->HasInstance(\\2)"
	"NanGetFunction/Nan::GetFunction"
	"Nan::Nan/Nan"
	"NanNan/Nan"
	"NanSet/Nan::Set"
	"NaN/Nan"
	"ToLocalChecked\\(\\).ToLocalChecked\\(\\)/ToLocalChecked()"
)

for file in $files; do
	echo $file
	for replacement in "${replacements[@]}"; do
		cat $file | sed -r "s/${replacement}/g" > $file.$$ && mv $file.$$ $file
	done
done
