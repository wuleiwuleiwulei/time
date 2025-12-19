/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import type { AsyncCallback } from '@ohos.base';
import { BusinessError } from '@ohos.base';

export default namespace systemDateTime {

  const SYSTEM_ERROR: int = 401

  loadLibraryWithPermissionCheck("systemdatetime_ani", "@ohos.systemDateTime")

  export function getCurrentTime(isNano: boolean, callback: AsyncCallback<number>): void {
    let p1 = taskpool.execute(getTime, isNano)
    p1.then((e :NullishType) => {
      let error = new BusinessError<void>
      error.code = 0
      callback(error, e as number)
    });
  }

  export function getCurrentTime(callback: AsyncCallback<number>): void {
    let p1 = taskpool.execute(getTime)
    p1.then((e :NullishType) => {
      let error = new BusinessError<void>
      error.code = 0
      callback(error, e as number)
    });
  }

  export function getCurrentTime(isNano?: boolean): Promise<number> {
    let p = new Promise<number>((resolve: (value: number) => void, reject: (error: Error) => void): void => {
      let p1 = taskpool.execute(getTime, isNano);
      p1.then((e: NullishType): void => {
        resolve(e as number);
      });
    });
    return p
  }

  export native function getTime(isNanoseconds?: boolean): number;
  export native function getTimezoneSync(): string;

  export function getTimezone(callback: AsyncCallback<string>): void{
    let p1 = taskpool.execute(getTimezoneSync)
    p1.then((e :NullishType) => {
      let s = e as string
      let error = new BusinessError<void>
      if (s === "") {
        error.code = SYSTEM_ERROR
      } else {
        error.code = 0
      }
      callback(error, s)
    });
  }

  export function getTimezone(): Promise<string> {
    let p = new Promise<string>((resolve: (value: string) => void, reject: (error: Error) => void): void => {
      let p1 = taskpool.execute(getTimezoneSync);
      p1.then((e: NullishType): void => {
        let s = e as string
        if (s === "") {
          let error = new BusinessError<void>
          error.code = SYSTEM_ERROR
          reject(error)
        } else {
          resolve(s);
        }
      });
    });
    return p
  }
}
